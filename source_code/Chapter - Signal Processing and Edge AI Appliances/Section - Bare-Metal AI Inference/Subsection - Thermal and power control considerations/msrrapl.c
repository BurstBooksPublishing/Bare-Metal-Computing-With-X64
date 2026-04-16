#include 

/* Read 64-bit MSR (privileged). Returns value. */
static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr"
                      : "=a"(lo), "=d"(hi)
                      : "c"(msr)
                      : );
    return ((uint64_t)hi << 32) | lo;
}

/* Write 64-bit MSR (privileged). */
static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr"
                      :
                      : "c"(msr), "a"(lo), "d"(hi)
                      : );
}

/* Parse MSR_RAPL_POWER_UNIT and compute energy scaling factor. */
double rapl_energy_unit(void)
{
    const uint32_t MSR_RAPL_POWER_UNIT = 0x606; /* model-specific */
    uint64_t v = rdmsr(MSR_RAPL_POWER_UNIT);
    uint32_t energy_bits = (v >> 8) & 0x1F; /* bits 12:8 */
    return pow(2.0, - (double)energy_bits);  /* joules per raw tick */
}

/* Read package energy status and return joules since last read. */
double read_package_energy_joules(uint64_t *prev_raw)
{
    const uint32_t MSR_PKG_ENERGY_STATUS = 0x611; /* model-specific */
    uint64_t raw = rdmsr(MSR_PKG_ENERGY_STATUS) & 0xFFFFFFFFULL;
    double unit = rapl_energy_unit();
    uint64_t delta_raw = (raw >= *prev_raw) ? raw - *prev_raw
                                           : (raw + (1ULL<<32)) - *prev_raw; /* wrap */
    *prev_raw = raw;
    return delta_raw * unit;
}