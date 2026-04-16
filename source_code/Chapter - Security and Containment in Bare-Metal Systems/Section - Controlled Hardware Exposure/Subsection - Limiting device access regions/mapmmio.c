#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096ULL
#define PTE_PRESENT  (1ULL<<0)
#define PTE_RW       (1ULL<<1)
#define PTE_USER     (1ULL<<2)
#define PTE_PWT      (1ULL<<3)
#define PTE_PCD      (1ULL<<4)
#define PTE_PAT      (1ULL<<7)
#define PTE_NX       (1ULL<<63)

extern uint64_t *pml4_base;            // root page-table base (physical-address mapped)
extern void *alloc_page(void);         // returns zeroed physical page mapped into kernel VA
static inline void invlpg(void *addr){ asm volatile("invlpg (%0)" :: "r"(addr) : "memory"); }

static inline unsigned idx_for_level(uint64_t va, int level){
    int shift = 12 + 9*(level-1);
    return (va >> shift) & 0x1FF;
}

void map_mmio_range(uint64_t vbase, uint64_t pbase, size_t len, int writable){
    size_t npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    for(size_t i=0;i<npages;i++){
        uint64_t va = vbase + i*PAGE_SIZE;
        uint64_t pa = pbase + i*PAGE_SIZE;
        // Walk PML4 -> PDPT -> PD -> PT, allocate tables as needed.
        uint64_t *pml4 = pml4_base;
        unsigned i4 = idx_for_level(va,4);
        uint64_t *pdpt = (uint64_t*) (pml4[i4] & ~0xFFFULL);
        if(!pdpt){ pdpt = alloc_page(); pml4[i4] = (uint64_t)pdpt | PTE_PRESENT | PTE_RW; }
        unsigned i3 = idx_for_level(va,3);
        uint64_t *pd = (uint64_t*)(pdpt[i3] & ~0xFFFULL);
        if(!pd){ pd = alloc_page(); pdpt[i3] = (uint64_t)pd | PTE_PRESENT | PTE_RW; }
        unsigned i2 = idx_for_level(va,2);
        uint64_t *pt = (uint64_t*)(pd[i2] & ~0xFFFULL);
        if(!pt){ pt = alloc_page(); pd[i2] = (uint64_t)pt | PTE_PRESENT | PTE_RW; }
        unsigned i1 = idx_for_level(va,1);
        uint64_t flags = PTE_PRESENT | PTE_PCD /*disable caching*/ | PTE_NX /*no execute*/;
        if(writable) flags |= PTE_RW;
        // Supervisor-only: do NOT set PTE_USER.
        pt[i1] = (pa & ~0xFFFULL) | flags;
        invlpg((void*)va); // ensure TLB consistency for this VA
    }
}