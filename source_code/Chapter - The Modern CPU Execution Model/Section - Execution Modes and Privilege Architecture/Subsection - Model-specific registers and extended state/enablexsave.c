#include 

// Read MSR (privileged)
static inline uint64_t read_msr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

// Write MSR (privileged)
static inline void write_msr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

// XGETBV/XSETBV wrappers (index 0 for XCR0)
static inline uint64_t xgetbv(unsigned int idx) {
    uint32_t lo, hi;
    asm volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(idx));
    return ((uint64_t)hi << 32) | lo;
}
static inline void xsetbv(unsigned int idx, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    asm volatile("xsetbv" : : "c"(idx), "a"(lo), "d"(hi));
}

// Enable OSXSAVE and set XCR0 bits for XMM (bit1) and YMM (bit2)
void enable_osxsave_avx(void) {
    uint64_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));            // read CR4
    cr4 |= (1ULL << 18);                                 // set OSXSAVE (CR4.OSXSAVE)
    asm volatile("mov %0, %%cr4" : : "r"(cr4));          // write CR4

    // Ensure CPUID reported XSAVE/AVX support before enabling
    // (Caller must check CPUID.{1}.ECX[27]=XSAVE, ECX[28]=AVX.)

    uint64_t xcr0 = xgetbv(0);                           // read XCR0
    xcr0 |= (1ULL << 1) | (1ULL << 2);                   // enable XMM and YMM state saves
    xsetbv(0, xcr0);                                     // write XCR0
}