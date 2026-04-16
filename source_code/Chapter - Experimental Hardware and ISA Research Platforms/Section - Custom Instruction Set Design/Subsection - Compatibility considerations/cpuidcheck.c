#include 

// Query CPUID: eax_in selects leaf; results returned in regs[4].
static inline void cpuid(uint32_t eax_in, uint32_t regs[4]) {
    __asm__ volatile("cpuid"
                     : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                     : "a"(eax_in), "c"(0));
}

// Example: feature bit is bit b of leaf base+1 (implementation-defined).
int cpu_has_custom_ext(uint32_t base_leaf, unsigned bit) {
    uint32_t regs[4];
    cpuid(base_leaf + 1, regs);
    return (regs[0] >> bit) & 1U; // eax contains feature mask here
}

// Guarded call: if extension present, use hardware; otherwise fallback.
void do_reduction(const float *data, size_t n, float *out,
                  uint32_t cpuid_leaf, unsigned feature_bit) {
    if (cpu_has_custom_ext(cpuid_leaf, feature_bit)) {
        // Inline call to hardware-accelerated routine (assembler wrapper).
        extern void hw_vector_reduce(const float*, size_t, float*);
        hw_vector_reduce(data, n, out);
    } else {
        // Portable fallback: deterministic scalar reduction.
        float acc = 0.0f;
        for (size_t i = 0; i < n; ++i) acc += data[i];
        *out = acc;
    }
}