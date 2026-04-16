#include 
#include 

// HPET register offsets
#define HPET_GCAP_ID       0x000
#define HPET_GEN_CONFIG    0x010
#define HPET_MAIN_COUNTER  0x0F0
#define HPET_TIMER0        0x100
#define HPET_TIMER_STRIDE  0x20

// Gen config bits
#define HPET_ENABLE        (1ULL << 0)
#define HPET_LEGACY_ROUTE  (1ULL << 1)

// Read 64-bit MMIO register
static inline uint64_t mmio_read64(void *base, size_t off) {
    volatile uint64_t *ptr = (volatile uint64_t *)((uint8_t *)base + off);
    return *ptr;
}
// Write 64-bit MMIO register
static inline void mmio_write64(void *base, size_t off, uint64_t v) {
    volatile uint64_t *ptr = (volatile uint64_t *)((uint8_t *)base + off);
    *ptr = v;
    __asm__ volatile ("" ::: "memory"); // compiler barrier
}

// Initialize HPET and return period in femtoseconds.
uint64_t hpet_init(void *hpet_mmio) {
    uint64_t gcap = mmio_read64(hpet_mmio, HPET_GCAP_ID);
    uint64_t period_fs = (gcap >> 32); // upper 32 bits: femtoseconds per tick

    // Disable HPET before modifying main counter per spec.
    uint64_t cfg = mmio_read64(hpet_mmio, HPET_GEN_CONFIG);
    cfg &= ~HPET_ENABLE;
    mmio_write64(hpet_mmio, HPET_GEN_CONFIG, cfg);

    // Zero main counter while disabled for deterministic start.
    mmio_write64(hpet_mmio, HPET_MAIN_COUNTER, 0ULL);

    // Enable HPET (do not enable legacy routing unless explicitly desired).
    cfg |= HPET_ENABLE;
    cfg &= ~HPET_LEGACY_ROUTE;
    mmio_write64(hpet_mmio, HPET_GEN_CONFIG, cfg);

    return period_fs;
}

// Busy-wait sleep for ms milliseconds using HPET main counter.
void hpet_sleep_ms(void *hpet_mmio, uint64_t period_fs, double ms) {
    // compute ticks required: ticks = ms/1000 * 1e15 / period_fs
    double ticks_d = (ms / 1000.0) * (1e15 / (double)period_fs);
    uint64_t ticks = (uint64_t)(ticks_d + 0.5);

    uint64_t start = mmio_read64(hpet_mmio, HPET_MAIN_COUNTER);
    uint64_t target = start + ticks;
    // wait for counter to reach target; wrap-around of 64-bit is practically impossible.
    while (mmio_read64(hpet_mmio, HPET_MAIN_COUNTER) < target) {
        __asm__ volatile ("pause" ::: "memory"); // reduce power/IO pressure
    }
}