#include 

// Read TSC with serialization (RDTSCP); returns tsc and aux (core id).
static inline uint64_t rdtscp(uint32_t *aux) {
    uint32_t hi, lo;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(*aux) :: "memory");
    return ((uint64_t)hi << 32) | lo;
}

// Critical sampling loop: caller must ensure buffers are page-locked and aligned.
void sample_loop(uint64_t periodTsc, volatile uint16_t *adcBuffer,
                 size_t samplesPerEpoch, uint16_t *workBuffer) {
    uint32_t aux;
    // disable interrupts to bound async preemption
    __asm__ volatile("cli" ::: "memory");
    for (size_t i = 0; i < samplesPerEpoch; ++i) {
        uint64_t t0 = rdtscp(&aux);
        // Deterministic read from ADC device memory (memory-mapped)
        uint16_t sample = adcBuffer[i];         // device read (cacheability set)
        workBuffer[i] = sample;                 // L1-resident store
        // busy-wait until next period using TSC
        uint64_t target = t0 + periodTsc;
        while (1) {
            uint32_t skip;
            uint64_t now = rdtscp(&skip);
            if (now >= target) break;
            __asm__ volatile("pause" ::: "memory"); // short backoff
        }
    }
    // re-enable interrupts
    __asm__ volatile("sti" ::: "memory");
}