#include 
#define IA32_APIC_BASE_MSR 0x1BULL
#define APIC_TPR_OFFSET    0x80u
#define APIC_LVT0_OFFSET   0x350u
#define APIC_LVT1_OFFSET   0x360u
#define APIC_LVT_MASK      (1u << 16)

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val, hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

/* Map APIC base physical to pointer (assumes identity mapping). */
volatile uint32_t *lapic_base(void) {
    uint64_t base = rdmsr(IA32_APIC_BASE_MSR) & 0xFFFFF000ULL;
    return (volatile uint32_t *)(uintptr_t)base;
}

/* Enter deterministic region: mask LVTs, raise TPR, then CLI. */
void enter_deterministic_region(void) {
    volatile uint32_t *apic = lapic_base();
    /* Mask LVT0 and LVT1 (if present). */
    apic[APIC_LVT0_OFFSET / 4] |= APIC_LVT_MASK;
    apic[APIC_LVT1_OFFSET / 4] |= APIC_LVT_MASK;
    /* Raise TPR to 0xFF to block lower-priority APIC interrupts. */
    apic[APIC_TPR_OFFSET / 4] = 0xFF;
    /* Disable maskable interrupts at CPU level. */
    __asm__ volatile ("cli" ::: "memory");
}

/* Exit deterministic region: STI then restore TPR and LVTs as needed. */
void exit_deterministic_region(void) {
    /* Re-enable maskable interrupts first to avoid races. */
    __asm__ volatile ("sti" ::: "memory");
    volatile uint32_t *apic = lapic_base();
    /* Lower TPR to 0 to allow normal priorities. */
    apic[APIC_TPR_OFFSET / 4] = 0x0;
    /* Unmask LVT entries if system policy requires it. */
    apic[APIC_LVT0_OFFSET / 4] &= ~APIC_LVT_MASK;
    apic[APIC_LVT1_OFFSET / 4] &= ~APIC_LVT_MASK;
}