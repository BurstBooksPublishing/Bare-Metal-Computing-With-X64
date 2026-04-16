#include 

// Basic MSR/RDPMC helpers (must run at CPL0)
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val, hi = (uint32_t)(val >> 32);
    asm volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}
static inline uint64_t rdpmc(uint32_t idx) {
    uint32_t lo, hi;
    asm volatile ("rdpmc" : "=a"(lo), "=d"(hi) : "c"(idx));
    return ((uint64_t)hi << 32) | lo;
}
static inline uint64_t rdtsc64(void) {
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#define IA32_PERFEVTSEL0 0x186u
#define IA32_PERF_GLOBAL_CTRL 0x38Fu

// Example: program PMC0 to count "unhalted core cycles" (event code 0x3C).
// Bits: [7:0] EventSelect, [15:8] UMASK, bit 16 USR, 17 OS, bit 22 ENABLE.
// Verify exact layout for your CPU model.
void setup_pmc0_for_core_cycles(void) {
    uint64_t event = 0x3Cu;          // architecture event code (model-specific)
    uint64_t umask = 0x00u;
    uint64_t config = event | (umask << 8) | (1ULL<<16) | (1ULL<<17) | (1ULL<<22);
    wrmsr(IA32_PERFEVTSEL0, config); // program event selector
    wrmsr(IA32_PERF_GLOBAL_CTRL, 1ULL); // enable PMC0
}

uint64_t sample_pmc0_delta(void) {
    uint64_t t0 = rdtsc64();
    uint64_t c0 = rdpmc(0);
    // ... run workload to measure ...
    uint64_t c1 = rdpmc(0);
    uint64_t t1 = rdtsc64();
    // return event count and elapsed cycles for converting to rate externally
    return c1 - c0;
}