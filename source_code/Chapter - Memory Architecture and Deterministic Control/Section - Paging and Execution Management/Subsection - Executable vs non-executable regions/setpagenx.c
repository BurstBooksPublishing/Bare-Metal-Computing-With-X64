#include 
#include 

#define NX_MASK (1ULL << 63)

/* Set NX bit for the PTE pointed to by pte_ptr for virtual address vaddr.
   Must be called at CPL0 in long mode with page tables writable. */
static inline void set_page_nx(void *vaddr, uint64_t *pte_ptr, bool enable_nx)
{
    uint64_t old = __atomic_load_n(pte_ptr, __ATOMIC_SEQ_CST); // read PTE atomically
    uint64_t nw = enable_nx ? (old | NX_MASK) : (old & ~NX_MASK);
    if (nw == old) return; // nothing to do

    __atomic_store_n(pte_ptr, nw, __ATOMIC_SEQ_CST); // publish new PTE

    /* Local TLB invalidation for the single page. */
    asm volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}