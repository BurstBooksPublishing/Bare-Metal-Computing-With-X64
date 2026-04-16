#include 
#define MMIO_BASE ((volatile uint32_t*)0xF0000000U) // device doorbell
#define DOORBELL_OFFSET 0x0
#define PAYLOAD_REG_OFFSET 0x4
#define SEED 0x12345678U
#define LCG_A 1664525U
#define LCG_C 1013904223U
#define LCG_M 0xFFFFFFFFU
#define TSC_PER_SEC 3000000000ULL // calibrated cycles per second
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static inline void mfence(void) { __asm__ volatile("mfence" ::: "memory"); }
/* Inject N deterministic payloads evenly spaced by interval_sec. */
void inject_stream(size_t N, double interval_sec) {
    uint32_t x = SEED;
    uint64_t t0 = rdtsc();
    uint64_t delta = (uint64_t)(interval_sec * (double)TSC_PER_SEC);
    for (size_t k = 0; k < N; ++k) {
        /* deterministic payload generation (LCG) */
        x = (uint32_t)(((uint64_t)LCG_A * x + LCG_C) & LCG_M);
        /* wait until target TSC */
        uint64_t target = t0 + k * delta;
        while (rdtsc() < target) { __asm__ volatile("pause"); }
        /* write payload then doorbell with ordering */
        MMIO_BASE[PAYLOAD_REG_OFFSET/4] = x;  // write payload
        mfence();                              // ensure device sees payload
        MMIO_BASE[DOORBELL_OFFSET/4] = 1U;     // ring doorbell
        mfence();                              // ensure write completes
    }
}