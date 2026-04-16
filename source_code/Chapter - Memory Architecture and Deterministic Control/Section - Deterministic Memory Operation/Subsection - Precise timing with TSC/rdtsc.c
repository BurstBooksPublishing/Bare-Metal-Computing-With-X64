#include 

/* Read ordered start timestamp: CPUID; RDTSC */
static inline uint64_t tsc_read_start(void) {
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(0));                 // serialize prior instructions
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi)); // read TSC
    return ((uint64_t)hi << 32) | lo;
}

/* Read ordered end timestamp: RDTSCP; CPUID */
static inline uint64_t tsc_read_end(void) {
    uint32_t aux, lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux)); // read TSC, ECX=aux
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(0));                 // serialize after rdtscp
    (void)aux; // optional use of IA32_TSC_AUX value
    return ((uint64_t)hi << 32) | lo;
}

/* Calibrate TSC frequency by sampling a reference timer callback.
   read_ref() returns a monotonically advancing reference counter.
   ref_freq is the reference counter frequency in Hz. */
static double calibrate_tsc(uint64_t (*read_ref)(void), double ref_freq) {
    uint64_t ref0 = read_ref();
    uint64_t t0   = tsc_read_start();
    /* spin or delay to create measurable interval */
    for (volatile int i = 0; i < 1000000; ++i) { __asm__ volatile("pause"); }
    uint64_t t1   = tsc_read_end();
    uint64_t ref1 = read_ref();
    double dt_ref = (double)(ref1 - ref0) / ref_freq;
    double dt_tsc = (double)(t1 - t0);
    return dt_tsc / dt_ref; // returns f_tsc in Hz
}