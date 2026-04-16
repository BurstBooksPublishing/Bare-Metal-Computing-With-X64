#include 

// Read TSC with serialization using RDTSCP (returns aux in ecx).
static inline uint64_t read_tsc_rdtscp(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    __asm__ volatile ("cpuid" ::: "rax","rbx","rcx","rdx"); // serialize after
    return ((uint64_t)hi << 32) | lo;
}

// Simple memory read for HPET registers (64-bit registers).
static inline uint64_t mmio_read64(volatile void *addr) {
    return *((volatile uint64_t *)addr);
}

// Calibrate TSC frequency using HPET. hpet_base points to MMIO base.
// Returns frequency in Hz as a double.
double calibrate_tsc_with_hpet(volatile void *hpet_base, uint64_t sample_hpet_ticks) {
    // HPET registers offsets
    const size_t GCAP_ID = 0x0;
    const size_t MAIN_COUNTER = 0xF0;

    uint64_t gcap = mmio_read64((volatile void *)((char*)hpet_base + GCAP_ID));
    uint64_t period_fs = gcap >> 32; // femtoseconds per HPET tick (architected)
    double period_s = (double)period_fs * 1e-15;

    // Initial samples (pin to core before calling this function).
    uint64_t t0 = read_tsc_rdtscp();
    uint64_t h0 = mmio_read64((volatile void *)((char*)hpet_base + MAIN_COUNTER));

    // Spin until desired HPET delta; in bare-metal use busy-wait to avoid interrupts.
    while (1) {
        uint64_t h1 = mmio_read64((volatile void *)((char*)hpet_base + MAIN_COUNTER));
        if (h1 - h0 >= sample_hpet_ticks) {
            uint64_t t1 = read_tsc_rdtscp();
            uint64_t delta_t = t1 - t0;
            uint64_t delta_h = h1 - h0;
            // f_TSC = delta_t / (delta_h * period_s)
            return (double)delta_t / ((double)delta_h * period_s);
        }
    }
}