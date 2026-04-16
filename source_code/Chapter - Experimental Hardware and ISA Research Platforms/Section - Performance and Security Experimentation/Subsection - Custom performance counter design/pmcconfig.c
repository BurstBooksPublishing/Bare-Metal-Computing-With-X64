#include 

// Basic MSR access (ring 0). // Returns low:high in registers.
static inline void rdmsr(uint32_t msr, uint64_t *value) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    *value = ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

// MSR bases (Intel conventional values).
#define IA32_PERFEVTSEL0 0x186u
#define IA32_PMC0        0xC1u
#define IA32_PERF_GLOBAL_CTRL 0x38Fu

// Configure programmable counter index 'idx' with event and umask.
void configure_pmc(unsigned idx, uint8_t event, uint8_t umask, uint8_t privilege_mask) {
    uint32_t evtsel_msr = IA32_PERFEVTSEL0 + idx;
    uint64_t evtsel = 0;
    evtsel |= (uint64_t)event;            // event select [7:0]
    evtsel |= (uint64_t)umask << 8;       // umask [15:8]
    evtsel |= (uint64_t)privilege_mask << 16; // privilege mask bits
    evtsel |= (1ULL << 22);               // enable counter (bit 22 per Intel)
    wrmsr(evtsel_msr, evtsel);
    // Enable global counter bit for this PMC.
    uint64_t gctrl;
    rdmsr(IA32_PERF_GLOBAL_CTRL, &gctrl);
    gctrl |= (1ULL << idx);
    wrmsr(IA32_PERF_GLOBAL_CTRL, gctrl);
}

// Read a 64-bit extended counter by software accumulation.
// Maintain 'high' externally to persist across reads.
uint64_t read_pmc_extended(unsigned idx, uint64_t *high) {
    uint32_t pmc_msr = IA32_PMC0 + idx;
    uint64_t low = 0;
    rdmsr(pmc_msr, &low);                  // hardware width typically 40/48 bits
    uint64_t mask = ((1ULL << 48) - 1);    // adjust to actual hw width if known
    uint64_t val = low & mask;
    // Detect wrap and extend.
    if (val < (*high & mask)) {
        *high += (1ULL << 48);             // increment high part on wrap
    }
    *high = (*high & ~mask) | val;
    return *high;
}