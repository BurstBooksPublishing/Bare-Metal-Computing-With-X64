#include 

/* Read MSR (IA32_APIC_BASE = 0x1B) */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr) );
    return ((uint64_t)hi << 32) | lo;
}

/* APIC register offsets */
enum { APIC_TPR = 0x80, APIC_EOI = 0x0B0 };

/* Obtain pointer to APIC MMIO region (assumes identity mapping) */
static inline volatile uint32_t *apic_base_ptr(void) {
    uint64_t msr = rdmsr(0x1B);                 /* \lstinline|IA32_APIC_BASE| */
    uint64_t base = msr & 0xFFFFF000ULL;        /* bits 35:12 hold base */
    return (volatile uint32_t *) (uintptr_t) base;
}

/* MMIO helpers (APIC regs are 32-bit, offsets in bytes) */
static inline void apic_write(uint32_t offset, uint32_t value) {
    volatile uint32_t *apic = apic_base_ptr();
    apic[offset >> 2] = value;
    asm volatile ("" ::: "memory");             /* ensure ordering */
}
static inline uint32_t apic_read(uint32_t offset) {
    volatile uint32_t *apic = apic_base_ptr();
    uint32_t v = apic[offset >> 2];
    asm volatile ("" ::: "memory");
    return v;
}

/* Set TPR to the class for vector v (blocks equal/lower classes) */
static inline void set_tpr_for_vector(uint8_t vector) {
    uint8_t cls = (vector >> 4) & 0x0F;         /* p(v) from Eq.~(1) */
    apic_write(APIC_TPR, ((uint32_t)cls) << 4); /* TPR holds class in high nibble */
}

/* Signal end of interrupt */
static inline void apic_eoi(void) {
    apic_write(APIC_EOI, 0);                    /* value ignored by APIC */
}