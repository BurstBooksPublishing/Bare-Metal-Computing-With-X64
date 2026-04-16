#include 
#include 

/* rdmsr/wrmsr for bare-metal x64 (requires CPL0 on real hardware). */
static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

/* RAPL MSRs */
enum { MSR_RAPL_POWER_UNIT = 0x606, MSR_PKG_POWER_LIMIT = 0x610, MSR_PKG_ENERGY_STATUS = 0x611 };

/* Read RAPL power unit, energy unit, and time unit (packed in MSR 0x606). */
static void read_rapl_units(double *power_unit, double *energy_unit, double *time_unit)
{
    uint64_t v = rdmsr(MSR_RAPL_POWER_UNIT);
    *power_unit  = 1.0 / (1ULL << (v & 0xF));           /* example encoding */
    *energy_unit = 1.0 / (1ULL << ((v >> 8) & 0x1F));
    *time_unit   = 1.0 / (1ULL << ((v >> 16) & 0xF));
}

/* Set package power limit (simplified: writes limit field directly). */
static void set_pkg_power_limit(double watts, double power_unit)
{
    /* Encode value in platform units (implementation-specific). */
    uint64_t raw = (uint64_t)(watts / power_unit) & 0x7FFF; /* 15-bit field */
    uint64_t msr = (raw) | (1ULL << 63); /* enable bit in high bit (platform dependent) */
    wrmsr(MSR_PKG_POWER_LIMIT, msr);
}

/* Read package energy counter and compute power over interval. */
static double measure_power(double energy_unit, unsigned int ms_delay)
{
    uint64_t e0 = rdmsr(MSR_PKG_ENERGY_STATUS);
    /* busy-wait delay (replace with timer on real platform) */
    for (volatile unsigned int i = 0; i < ms_delay * 1000; ++i) __asm__ volatile("nop");
    uint64_t e1 = rdmsr(MSR_PKG_ENERGY_STATUS);
    uint64_t delta = (e1 - e0) & 0xffffffffULL; /* wrap-safe 32-bit counter */
    double energy_joules = delta * energy_unit;
    double seconds = ms_delay / 1000.0;
    return energy_joules / seconds;
}

/* Main loop: enforce budget and throttle accelerator MMIO. */
void power_control_loop(volatile uint32_t *accel_ctrl, double budget_watts)
{
    double power_u, energy_u, time_u;
    read_rapl_units(&power_u, &energy_u, &time_u);
    set_pkg_power_limit(budget_watts, power_u);
    for (;;) {
        double p = measure_power(energy_u, 200); /* 200 ms window */
        if (p > budget_watts) {
            /* signal accelerator to reduce parallelism (vendor protocol) */
            *accel_ctrl = 0x1; /* example: 1 => low-parallel mode */
            /* tighten package cap to force CPU throttling as backup */
            set_pkg_power_limit(budget_watts * 0.95, power_u);
        } else {
            *accel_ctrl = 0x0; /* normal mode */
            set_pkg_power_limit(budget_watts, power_u);
        }
        /* small sleep or wait for timer interrupt in a full runtime */
        for (volatile int i = 0; i < 2000000; ++i) __asm__ volatile("nop");
    }
}