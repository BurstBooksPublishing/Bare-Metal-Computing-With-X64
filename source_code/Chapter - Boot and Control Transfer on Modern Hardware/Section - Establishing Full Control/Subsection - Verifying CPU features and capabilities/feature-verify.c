#include 
#include 

/* CPUID: returns registers in outputs. */
static inline void cpuid(uint32_t eax_in, uint32_t ecx_in,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx) {
    uint32_t a,b,c,d;
    __asm__ volatile("cpuid"
                     : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(eax_in), "c"(ecx_in));
    if (eax) *eax = a; if (ebx) *ebx = b; if (ecx) *ecx = c; if (edx) *edx = d;
}

/* XGETBV: returns XCR0 (RDX:RAX). */
static inline uint64_t xgetbv(uint32_t index) {
    uint32_t a,d;
    __asm__ volatile(".byte 0x0f,0x01,0xd0" /* xgetbv opcode */
                     : "=a"(a), "=d"(d) : "c"(index));
    return ((uint64_t)d << 32) | a;
}

/* RDMSR: read 64-bit MSR. Must run at CPL0. */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

/* Read CR4 (CPL0). */
static inline uint64_t read_cr4(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

/* High-level checks */
bool cpu_has_long_mode(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000001) return false;
    cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    return (edx >> 29) & 1; /* CPUID.80000001.EDX.LM */
}

bool long_mode_enabled(void) {
    const uint32_t IA32_EFER = 0xC0000080;
    uint64_t efer = rdmsr(IA32_EFER);
    const uint64_t LME_BIT = (1ULL << 8); /* LME is bit 8 in IA32_EFER */
    if ((efer & LME_BIT) == 0) return false;
    /* CR4.PAE (bit 5) must be set before entering long mode */
    return (read_cr4() & (1ULL << 5)) != 0;
}

bool avx_available_and_enabled(void) {
    uint32_t a,b,c,d;
    cpuid(1,0,&a,&b,&c,&d);
    const uint32_t AVX_BIT = (1u << 28);
    const uint32_t OSXSAVE_BIT = (1u << 27);
    if ((c & AVX_BIT) == 0) return false;
    if ((c & OSXSAVE_BIT) == 0) return false;
    uint64_t xcr0 = xgetbv(0);
    return (xcr0 & 0x6) == 0x6; /* XCR0[2:1] == 1 for SSE+AVX */
}