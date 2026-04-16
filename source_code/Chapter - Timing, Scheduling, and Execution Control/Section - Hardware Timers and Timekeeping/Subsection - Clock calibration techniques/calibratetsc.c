#include 
#include 

static inline uint64_t rdtscp_serial(void) {
    uint32_t aux;
    uint64_t rax, rdx;
    asm volatile("rdtscp" : "=a"(rax), "=d"(rdx), "=c"(aux) ::);
    return (rdx << 32) | rax;
}

/* Read HPET main counter (64-bit). hpet_base is physical MMIO base mapped to virtual space. */
static inline uint64_t hpet_read64(volatile void *hpet_base) {
    volatile uint32_t *p = (volatile uint32_t *)hpet_base;
    uint32_t lo = p[0x0 / 4];              // offset 0x0: main counter low
    uint32_t hi = p[0x4 / 4];              // offset 0x4: main counter high
    return ((uint64_t)hi << 32) | lo;
}

/* Calibrate TSC:
   - hpet_base: mapped MMIO address of HPET
   - hpet_period_fs: HPET period in femtoseconds (from HPET_CAPS register)
   - samples: number of samples to collect
   - out_freq: pointer to write estimated tsc frequency (Hz)
   - out_alpha, out_beta: fit coefficients for eq. (1)
*/
int calibrate_tsc_with_hpet(volatile void *hpet_base, uint64_t hpet_period_fs,
                            size_t samples, double *out_freq,
                            double *out_alpha, double *out_beta) {
    if (samples < 4) return -1;
    double *T = (double*)0; /* allocate or use stack in real code; omitted for brevity */
    /* For production, allocate arrays dynamically and handle memory; simplified here. */

    /* Collect samples */
    for (size_t i = 0; i < samples; ++i) {
        uint64_t r = hpet_read64(hpet_base);      // HPET ticks
        uint64_t t = rdtscp_serial();             // TSC ticks
        /* Convert HPET ticks to seconds: ticks * period_fs * 1e-15 */
        double rsec = (double)r * ((double)hpet_period_fs * 1e-15);
        /* store pair (T_i, R_i) into preallocated arrays (omitted) */
    }

    /* Compute OLS per eq. (2). Implement numerically stable accumulation (Kahan or two-pass). */
    /* On success, set *out_freq = 1.0 / beta; *out_alpha = alpha; *out_beta = beta */
    return 0;
}