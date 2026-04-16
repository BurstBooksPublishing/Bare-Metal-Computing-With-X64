#include 
#include 

// Minimal flags for entries.
#define PTE_PRESENT  (1ULL<<0)
#define PTE_RW       (1ULL<<1)
#define PTE_PWT      (1ULL<<3)
#define PTE_PCD      (1ULL<<4)
#define PTE_PS       (1ULL<<7)

// Page sizes and masks.
#define PAGE_4K      0x1000ULL
#define PAGE_2M      0x200000ULL
#define PAGE_MASK(size) (~((size)-1))

// Platform must provide:
// - allocate_page(): returns a 4KiB zeroed physical frame for page tables.
// - phys_to_virt(p): converts a physical address of a page-table frame to a virtual pointer.
extern uint64_t allocate_page(void);
extern void *phys_to_virt(uint64_t paddr);

// Walks table and returns pointer to entry, allocating intermediates.
static uint64_t *walk_create(uint64_t *pml4, uint64_t vaddr, int level) {
    // level: 4=PML4,3=PDPT,2=PD,1=PT. We return pointer to entry at 'level'.
    int idx = (vaddr >> ((level-1)*9 + 12)) & 0x1FF;
    uint64_t ent = pml4[idx];
    if (!(ent & PTE_PRESENT)) {
        uint64_t frame = allocate_page();
        pml4[idx] = frame | PTE_PRESENT | PTE_RW;
        ent = pml4[idx];
    }
    return (uint64_t *)phys_to_virt(pml4[idx] & ~0xFFFULL);
}

// Map MMIO range [vbase, vbase+size) to [pbase, pbase+size).
int map_mmio_range(uint64_t *pml4, uint64_t vbase, uint64_t pbase, size_t size) {
    uint64_t end = vbase + size;
    uint64_t va = vbase;
    uint64_t pa = pbase;

    while (va < end) {
        // Try 2MiB mapping when both va and pa aligned and remaining >= 2MiB.
        if ((va & (PAGE_2M-1)) == 0 && (pa & (PAGE_2M-1)) == 0 && (end - va) >= PAGE_2M) {
            // Walk to PD entry.
            uint64_t *pdpt = walk_create(pml4, va, 3);
            uint64_t *pd   = walk_create(pdpt, va, 2);
            int pdi = (va >> 21) & 0x1FF;
            // Set PD entry: frame address | flags | PS
            pd[pdi] = (pa & PAGE_MASK(PAGE_2M)) | PTE_PRESENT | PTE_RW | PTE_PCD | PTE_PWT | PTE_PS;
            va += PAGE_2M;
            pa += PAGE_2M;
            continue;
        }

        // Fallback to 4KiB mappings.
        uint64_t *pdpt = walk_create(pml4, va, 3);
        uint64_t *pd   = walk_create(pdpt, va, 2);
        uint64_t *pt   = walk_create(pd, va, 1);
        int pti = (va >> 12) & 0x1FF;
        pt[pti] = (pa & PAGE_MASK(PAGE_4K)) | PTE_PRESENT | PTE_RW | PTE_PCD | PTE_PWT;
        va += PAGE_4K;
        pa += PAGE_4K;
    }
    // Invalidate TLB for the mapped range if running (use INVLPG per-page or CR3 reload).
    return 0;
}