/* Minimal APIC timer helpers for bare-metal x64. */
#include 

/* APIC physical base (typical); ensure identity-mapped or mapped into page tables. */
#define APIC_BASE_PHYS 0xFEE00000UL

/* Offsets (bytes) */
enum { APIC_SVR = 0xF0, APIC_EOI = 0xB0,
       APIC_LVT_TIMER = 0x320, APIC_INIT_COUNT = 0x380,
       APIC_CUR_COUNT = 0x390, APIC_DIV_CONF = 0x3E0 };

/* LVT Timer flags */
#define LVT_MASK     0x00010000U  /* mask timer */
#define LVT_PERIODIC 0x00020000U  /* periodic mode */

/* Helper pointer to APIC MMIO (32-bit registers). */
static volatile uint32_t *lapic = (volatile uint32_t *)APIC_BASE_PHYS;

/* Ensure write completion by a read-back. */
static inline void lapic_write(uint32_t offset, uint32_t value) {
    lapic[offset / 4] = value;
    (void)lapic[APIC_ID / 4]; /* read-back; APIC_ID is 0x20 but not used elsewhere */
}
/* Read MMIO register */
static inline uint32_t lapic_read(uint32_t offset) {
    return lapic[offset / 4];
}

/* Read TSC (for optional calibration). */
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((uint64_t)hi << 32) | lo;
}

/* Enable local APIC globally (set SVR bit 8). */
static inline void lapic_enable(void) {
    uint32_t svr = lapic_read(APIC_SVR);
    svr |= 0x100U; /* set APIC enable bit */
    lapic_write(APIC_SVR, svr);
}

/* Set divide configuration using the encoding value from Intel mapping. */
static inline void lapic_set_divide(uint32_t enc) {
    lapic_write(APIC_DIV_CONF, enc & 0xF);
}

/* Program the LAPIC timer into periodic mode with a given initial count. */
/* vector: interrupt vector to deliver (e.g., 0x40). initial_count: 32-bit start. */
static inline void lapic_program_timer(uint8_t vector, uint32_t initial_count) {
    /* Unmask and set periodic mode with the vector. */
    uint32_t lvt = (uint32_t)vector | LVT_PERIODIC;
    lapic_write(APIC_LVT_TIMER, lvt);
    lapic_write(APIC_INIT_COUNT, initial_count);
}

/* Public helper: set timer to period_us microseconds, given known apic_freq_hz */
/* choose divisor encoding for best fit; here we pick divisor=64 (encoding 0x9) as default. */
static inline void lapic_timer_set_period_us(uint8_t vector,
                                             uint32_t period_us,
                                             uint64_t apic_freq_hz) {
    uint32_t D_enc = 0x9; /* divisor = 64 */
    uint32_t D = 64;
    lapic_set_divide(D_enc);
    /* Compute count using equation C = T * f / D, with T in seconds. */
    uint64_t counts = (uint64_t)period_us * (apic_freq_hz / 1000000ULL) / D;
    if (counts == 0) counts = 1;
    lapic_program_timer(vector, (uint32_t)counts);
}

/* Write EOI from interrupt handler to clear in-service bit. */
static inline void lapic_send_eoi(void) {
    lapic_write(APIC_EOI, 0);
}