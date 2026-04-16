#include 

// PTE flag constants
#define PTE_PRESENT  (1ULL<<0)
#define PTE_WRITABLE (1ULL<<1)
#define PTE_USER     (1ULL<<2)
#define PTE_PWT      (1ULL<<3)
#define PTE_PCD      (1ULL<<4)
#define PTE_ACCESSED (1ULL<<5)
#define PTE_DIRTY    (1ULL<<6)
#define PTE_GLOBAL   (1ULL<<8)
#define PTE_NX       (1ULL<<63)

// Update a PTE atomically, then invalidate the TLB entry for va.
// pte_ptr points to the 64-bit page-table entry in memory.
// new_flags must include the physical frame address bits.
static inline void update_pte_and_flush(uint64_t *pte_ptr, uint64_t new_val, void *va)
{
    // Ensure the value is committed to memory before INVLPG.
    __atomic_store_n(pte_ptr, new_val, __ATOMIC_SEQ_CST);
    // Invalidate TLB entry for the virtual address.
    asm volatile("invlpg (%0)" :: "r"(va) : "memory");
    // Prevent reordering of subsequent memory operations.
    asm volatile("" ::: "memory");
}