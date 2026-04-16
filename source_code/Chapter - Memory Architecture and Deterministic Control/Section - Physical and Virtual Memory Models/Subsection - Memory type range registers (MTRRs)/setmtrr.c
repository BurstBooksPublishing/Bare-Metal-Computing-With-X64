#include 

// MSR addresses
#define MSR_MTRRCAP   0x000000FEu
#define MSR_MTRR_DEF  0x000002FFu
#define MSR_VAR_BASE  0x00000200u  // base index for variable MTRRs

// Helpers: rdmsr/wrmsr
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val, hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

// CR0 helpers and WBINVD
static inline uint64_t read_cr0(void) { uint64_t v; __asm__ volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static inline void write_cr0(uint64_t v) { __asm__ volatile("mov %0, %%cr0" :: "r"(v)); }
static inline void wbinvd(void) { __asm__ volatile("wbinvd" ::: "memory"); }

// Set one variable MTRR (index in [0, var_count-1]).
// type follows Intel encoding (0=UC,4=WT,6=WB typical).
int set_variable_mtrr(unsigned idx, uint64_t phys_base, uint64_t size, uint8_t type) {
    uint64_t cap = rdmsr(MSR_MTRRCAP);
    unsigned var_count = (unsigned)(cap & 0xFF);
    if (idx >= var_count) return -1;

    // Align base and size to 4KiB and require power-of-two size
    if ((phys_base & 0xFFF) || (size & 0xFFF)) return -2;
    if ((size & (size - 1)) != 0) return -3;

    // Disable caching: set CR0.CD (bit30) and CR0.NW (bit29)
    uint64_t cr0 = read_cr0();
    write_cr0(cr0 | (1ull<<30) | (1ull<<29));
    wbinvd();

    // Disable MTRRs by clearing enable in IA32_MTRR_DEF_TYPE (bit 11).
    // Preserve default type byte and clear the enable bits.
    uint64_t def = rdmsr(MSR_MTRR_DEF);
    uint64_t def_type = def & 0xFF;
    wrmsr(MSR_MTRR_DEF, def_type); // clear FE/E bits

    // Compute MSR indices for this variable MTRR
    uint32_t msr_base = MSR_VAR_BASE + (uint32_t)(idx * 2);
    uint32_t msr_mask = msr_base + 1;

    // PHYSBASE: base aligned, OR type in low 8 bits
    uint64_t physbase_msr = (phys_base & 0xFFFFFFFFFFFFF000ULL) | (uint64_t)type;

    // PHYSMASK: mask per eq. (1<<11) = V bit
    uint64_t physmask_msr = ((~(size - 1)) & 0xFFFFFFFFFFFFF000ULL) | (1ull<<11);

    wrmsr(msr_base, physbase_msr);
    wrmsr(msr_mask, physmask_msr);

    // Re-enable MTRRs: set def type and enable bit (bit11)
    wrmsr(MSR_MTRR_DEF, def_type | (1ull<<11)); // set E=1, keep default type

    // Restore caches
    write_cr0(cr0);
    return 0;
}