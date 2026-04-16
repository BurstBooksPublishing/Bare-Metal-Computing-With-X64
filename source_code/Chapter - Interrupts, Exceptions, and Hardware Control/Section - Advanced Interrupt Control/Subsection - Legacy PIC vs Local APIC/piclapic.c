#include 

/* Port I/O primitives (GCC inline asm). */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* MSR access (rdmsr/wrmsr). */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    asm volatile ("wrmsr" : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

/* Remap legacy PIC to offsets 'offset1' (master) and 'offset2' (slave). */
void pic_remap(int offset1, int offset2) {
    uint8_t a1 = inb(0x21); /* save masks */
    uint8_t a2 = inb(0xA1);

    outb(0x20, 0x11);            /* ICW1: start init, cascade mode */
    outb(0xA0, 0x11);
    outb(0x21, (uint8_t)offset1);/* ICW2: master vector offset */
    outb(0xA1, (uint8_t)offset2);/* ICW2: slave vector offset */
    outb(0x21, 0x04);            /* ICW3: tell master about slave at IRQ2 */
    outb(0xA1, 0x02);            /* ICW3: tell slave its cascade identity */
    outb(0x21, 0x01);            /* ICW4: 8086/88 (MCS-80/85) mode */
    outb(0xA1, 0x01);

    outb(0x21, a1);              /* restore saved masks */
    outb(0xA1, a2);
}

/* Enable Local APIC by setting IA32_APIC_BASE MSR and SVR. */
void enable_local_apic(uint8_t spurious_vector) {
    const uint32_t IA32_APIC_BASE = 0x1B;
    uint64_t apic_msr = rdmsr(IA32_APIC_BASE);

    /* Set base to 0xFEE00000 (bits 12..35) and set global enable bit (bit 11). */
    uint64_t base = (0xFEE00000ULL & 0xFFFFF000ULL);
    uint64_t new_msr = (apic_msr & 0xFFF) | (base & ~0xFFFULL) | (1ULL << 11);
    wrmsr(IA32_APIC_BASE, new_msr);

    volatile uint32_t *lapic = (volatile uint32_t *)0xFEE000000ULL; /* identity mapped */
    /* SVR (offset 0xF0): set vector and enable bit (bit 8 = 0x100) */
    lapic[0xF0 / 4] = ((uint32_t)spurious_vector) | 0x100;
}

/* Typical usage in early init:
   pic_remap(32, 40);            // move PIC IRQs to vectors 32..47
   enable_local_apic(0xFF);      // set spurious vector to 255 and enable APIC
*/