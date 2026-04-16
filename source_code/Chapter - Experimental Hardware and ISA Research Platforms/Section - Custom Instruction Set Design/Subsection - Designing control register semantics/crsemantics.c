#include 

// Read MSR (returns 64-bit value).
static inline uint64_t read_msr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr"
                      : "=a"(lo), "=d"(hi)
                      : "c"(msr)
                      : "memory");
    return ((uint64_t)hi << 32) | lo;
}

// Write MSR (64-bit value).
static inline void write_msr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr"
                      :
                      : "c"(msr), "a"(lo), "d"(hi)
                      : "memory");
}

// Atomically set IA32_EFER.LME (bit 8) and return previous value.
uint64_t enable_efer_lme(void)
{
    const uint32_t IA32_EFER = 0xC0000080u;
    uint64_t efer = read_msr(IA32_EFER);
    uint64_t new = efer | (1ULL << 8); // set LME
    if (new != efer) write_msr(IA32_EFER, new);
    return efer;
}

// Set CR3 to new_page_table base. Caller must be CPL0.
// Writing CR3 serializes translations and invalidates TLB as defined.
static inline void set_cr3(uint64_t new_cr3)
{
    __asm__ volatile (
        "mov %0, %%cr3\n\t"   // privileged write; causes required flush
        : /* no outputs */
        : "r"(new_cr3)
        : "memory");
}