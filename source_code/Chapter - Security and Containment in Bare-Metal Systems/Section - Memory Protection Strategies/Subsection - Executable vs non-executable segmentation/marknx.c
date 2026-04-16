#include 

// Read/Write IA32_EFER MSR to enable NXE (bit 11).
static inline void enable_nxe(void) {
    uint32_t lo, hi;
    // IA32_EFER = 0xC0000080
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000080));
    uint64_t val = ((uint64_t)hi << 32) | lo;
    val |= (1ULL << 11); // set NXE
    lo = (uint32_t)val; hi = (uint32_t)(val >> 32);
    asm volatile("wrmsr" :: "c"(0xC0000080), "a"(lo), "d"(hi));
}

// Caller must supply a function that walks the current page tables
// and returns a pointer to the 64-bit PTE for vaddr.
extern uint64_t *get_pte_ptr(void *vaddr);

void mark_page_nx(void *vaddr) {
    uint64_t *pte = get_pte_ptr(vaddr); // production implementation required
    const uint64_t NX_BIT = 1ULL << 63;
    // Atomically set NX and ensure visibility to other CPUs.
    __atomic_fetch_or(pte, NX_BIT, __ATOMIC_SEQ_CST);
    // Invalidate the TLB entry for vaddr.
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}