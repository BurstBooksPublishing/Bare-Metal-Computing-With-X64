#include 

/* Read TSC with serialization: CPUID; RDTSC */
static inline uint64_t read_tsc_start(void) {
    uint32_t lo, hi;
    asm volatile(
        "cpuid\n\t"             /* serialize prior instructions */
        "rdtsc\n\t"             /* read tsc */
        : "=a"(lo), "=d"(hi)
        : "a"(0)
        : "rbx", "rcx");
    return ((uint64_t)hi << 32) | lo;
}

/* Read TSC with serialization: RDTSCP; copy registers; CPUID */
static inline uint64_t read_tsc_end(void) {
    uint32_t lo, hi;
    asm volatile(
        "rdtscp\n\t"            /* read tsc and tsc_aux into ecx */
        "mov %%eax, %0\n\t"     /* move low */
        "mov %%edx, %1\n\t"     /* move high */
        "cpuid\n\t"             /* serialize following instructions */
        : "=r"(lo), "=r"(hi)
        :
        : "rax", "rbx", "rcx", "rdx");
    return ((uint64_t)hi << 32) | lo;
}

/* Simple in-place insertion sort for small arrays (deterministic) */
static void sort_u64(uint64_t *a, int n) {
    for (int i = 1; i < n; ++i) {
        uint64_t v = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > v) { a[j+1] = a[j]; --j; }
        a[j+1] = v;
    }
}

/* Measure function execution cycles, return median corrected cycles.
   - fn: function to time (should be small, no varargs)
   - runs: number of samples (odd preferred) */
uint64_t measure_median_cycles(void (*fn)(void), int runs) {
    if (runs < 3) runs = 3;
    uint64_t samples[runs];
    /* Calibrate overhead H by timing an empty marker */
    uint64_t H_samples[runs];
    for (int i = 0; i < runs; ++i) {
        uint64_t s = read_tsc_start();
        uint64_t e = read_tsc_end();
        H_samples[i] = e - s;
    }
    sort_u64(H_samples, runs);
    uint64_t H = H_samples[runs/2]; /* median overhead */

    for (int i = 0; i < runs; ++i) {
        uint64_t s = read_tsc_start();
        fn(); /* measured region */
        uint64_t e = read_tsc_end();
        uint64_t d = (e - s);
        samples[i] = (d > H) ? (d - H) : 0;
    }
    sort_u64(samples, runs);
    return samples[runs/2]; /* median corrected cycles */
}