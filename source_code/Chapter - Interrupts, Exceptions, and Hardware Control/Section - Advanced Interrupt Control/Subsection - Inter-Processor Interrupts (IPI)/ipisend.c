#include 

#define APIC_BASE_PHYS 0xFEE00000u
#define ICR_LOW  0x300u
#define ICR_HIGH 0x310u
#define MSR_X2APIC_ICR 0x830u

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
    *((volatile uint32_t *)addr) = val;
}
static inline uint32_t mmio_read32(uintptr_t addr) {
    return *((volatile uint32_t *)addr;
}

/* Poll until ICR delivery status clears (xAPIC). */
static void apic_wait_delivery(uintptr_t apic_base) {
    while (mmio_read32(apic_base + ICR_LOW) & (1u << 12)) { /* delivery status bit */ }
}

/* Send IPI using xAPIC (MMIO APIC). */
void xapic_send_ipi(uint8_t dest_apic_id, uint8_t vector, uint8_t delivery_mode) {
    uintptr_t base = (uintptr_t)APIC_BASE_PHYS;
    uint32_t icrhi = ((uint32_t)dest_apic_id) << 24; /* destination in high dword bits 56:63 become 24:31 of ICRHI in 32-bit write */
    uint32_t icrlo = ((uint32_t)vector) | ((uint32_t)delivery_mode << 8);

    mmio_write32(base + ICR_HIGH, icrhi);   /* set destination */
    mmio_write32(base + ICR_LOW, icrlo);    /* issue command */
    apic_wait_delivery(base);               /* wait completion */
}

/* Write MSR (for x2APIC). */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"(low), "d"(high) : "memory");
}

/* Send IPI using x2APIC (single MSR write). */
void x2apic_send_ipi(uint32_t dest, uint8_t vector, uint8_t delivery_mode) {
    uint64_t icr = (uint64_t)vector | ((uint64_t)delivery_mode << 8) | ((uint64_t)dest << 32);
    wrmsr(MSR_X2APIC_ICR, icr); /* atomic from software view */
}