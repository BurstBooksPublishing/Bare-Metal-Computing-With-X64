/* Minimal, production-oriented mapping helper for x86_64 long mode.
   Assumes:
   - PML4 base physical address is known and identity-mapped at 'pml4'.
   - Caller runs with paging enabled and CR3 pointing to 'pml4'.
   - phys_base and virt_base are 2MiB-aligned where possible.
   - No concurrency; interrupts should be disabled while modifying tables. */

#include 
#include 

#define PTE_PRESENT   (1ULL<<0)
#define PTE_RW        (1ULL<<1)
#define PTE_USER      (1ULL<<2)
#define PTE_PWT       (1ULL<<3)
#define PTE_PCD       (1ULL<<4)
#define PTE_ACCESSED  (1ULL<<5)
#define PTE_DIRTY     (1ULL<<6)
#define PTE_PSE       (1ULL<<7)       /* Page-size (for PD entries = 2MiB) */
#define PTE_GLOBAL    (1ULL<<8)
#define PTE_NX        (1ULL<<63)

static inline void invlpg(void *addr) {
    asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

void map_2mb_range(uint64_t *pml4, uint64_t phys_base, uint64_t virt_base,
                   size_t size, uint64_t flags)
{
    const uint64_t PAGE_2MB = 2ULL * 1024 * 1024;
    size_t pages = (size + PAGE_2MB - 1) / PAGE_2MB;
    for (size_t i = 0; i < pages; ++i) {
        uint64_t va = virt_base + i * PAGE_2MB;
        uint64_t pa = phys_base + i * PAGE_2MB;

        /* Walk indices: PML4[47:39], PDP[38:30], PD[29:21] */
        uint64_t idx_pml4 = (va >> 39) & 0x1FF;
        uint64_t idx_pdp  = (va >> 30) & 0x1FF;
        uint64_t idx_pd   = (va >> 21) & 0x1FF;

        uint64_t *pdp = (uint64_t*)( (uintptr_t)pml4[idx_pml4] & ~0xFFFULL );
        if (!pdp) {
            /* allocate new PDP page; implementation-specific allocator required */
            /* set pml4[idx_pml4] = physical_of_new_pdp | flags */
            return; /* placeholder: production code must allocate and set */
        }
        uint64_t *pd = (uint64_t*)( (uintptr_t)pdp[idx_pdp] & ~0xFFFULL );
        if (!pd) {
            /* allocate new PD page and install */
            return; /* placeholder */
        }

        /* Install 2MiB page: PD entry with PSE bit set and address aligned. */
        uint64_t pde = (pa & ~0x1FFFFFULL) | PTE_PRESENT | PTE_PSE | flags;
        pd[idx_pd] = pde;

        /* Invalidate TLB entry for this VA */
        invlpg((void*)va);
    }
    /* Optional: full TLB flush by reloading CR3 for large mappings */
    /* uint64_t cr3; asm volatile("mov %%cr3,%0":"=r"(cr3)); asm volatile("mov %0,%%cr3"::"r"(cr3)); */
}