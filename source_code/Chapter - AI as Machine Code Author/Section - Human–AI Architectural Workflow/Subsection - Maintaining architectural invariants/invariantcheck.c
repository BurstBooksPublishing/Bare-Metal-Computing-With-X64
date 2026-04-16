#include 

// Read control registers and MSR (freestanding, privileged context).
static inline uint64_t read_cr0(void) { uint64_t v; asm volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static inline uint64_t read_cr3(void) { uint64_t v; asm volatile("mov %%cr3, %0" : "=r"(v)); return v; }
static inline uint64_t read_cr4(void) { uint64_t v; asm volatile("mov %%cr4, %0" : "=r"(v)); return v; }
static inline uint64_t read_msr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline uint64_t read_rsp(void) { uint64_t v; asm volatile("mov %%rsp, %0" : "=r"(v)); return v; }

// Test for canonical 48-bit canonical addresses.
static inline int is_canonical(uint64_t addr){
    const uint64_t high_mask = 0xFFFF800000000000ULL;
    return ((addr & high_mask) == 0) || ((addr & high_mask) == high_mask);
}

// Return 0 on success, negative error codes for specific invariant violation.
int check_arch_invariants(void){
    uint64_t cr0 = read_cr0();
    uint64_t cr3 = read_cr3();
    uint64_t cr4 = read_cr4();
    uint64_t efer = read_msr(0xC0000080); // IA32_EFER
    uint64_t rsp = read_rsp();

    // 1) Stack alignment: must be 16-byte aligned at ABI boundaries.
    if (rsp & 0xFULL) return -1;

    // 2) CR3 must be canonical.
    if (!is_canonical(cr3)) return -2;

    // 3) NXE (bit 11) should be present for expected NX semantics.
    if (!(efer & (1ULL << 11))) return -3;

    // 4) If paging enabled (CR0.PG bit 31), enforce PAE and LME for long-mode correctness.
    if (cr0 & (1ULL << 31)) { // PG
        if (!(cr4 & (1ULL << 5))) return -4; // CR4.PAE
        if (!(efer & (1ULL << 8))) return -5; // EFER.LME
    }
    return 0;
}