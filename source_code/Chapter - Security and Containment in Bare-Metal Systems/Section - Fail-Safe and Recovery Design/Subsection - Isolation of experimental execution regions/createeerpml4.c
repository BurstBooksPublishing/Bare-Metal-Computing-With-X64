#include <stdint.h>
#include <stddef.h>

// Page flags
#define PTE_PRESENT   (1ULL<<0)
#define PTE_RW        (1ULL<<1)
#define PTE_USER      (1ULL<<2)
#define PTE_PWT       (1ULL<<3)
#define PTE_PCD       (1ULL<<4)
#define PTE_ACCESSED  (1ULL<<5)
#define PTE_DIRTY     (1ULL<<6)
#define PTE_PS        (1ULL<<7)   // 2MiB
#define PTE_GLOBAL    (1ULL<<8)
#define PTE_NX        (1ULL<<63)

static inline uint64_t align_down(uint64_t a, uint64_t b){ return a & ~(b-1); }

extern uint64_t tm_alloc_page_phys(void);

static inline void load_cr3(uint64_t phys){
    __asm__ volatile("mov %0, %%cr3" :: "r"(phys) : "memory");
}

uint64_t build_eer_pml4_2mb(uint64_t phys_base, size_t size, int exec){
    const uint64_t PAGE_2M = 0x200000;

    phys_base = align_down(phys_base, PAGE_2M);
    size = ((size + PAGE_2M - 1) / PAGE_2M) * PAGE_2M;
    uint64_t pages2m = size / PAGE_2M;

    uint64_t pml4_phys = tm_alloc_page_phys();
    uint64_t pdpt_phys = tm_alloc_page_phys();
    uint64_t pd_phys   = tm_alloc_page_phys();

    uint64_t *pml4 = (uint64_t*)(uintptr_t)(pml4_phys + 0xFFFFFFFF80000000ULL);
    uint64_t *pdpt = (uint64_t*)(uintptr_t)(pdpt_phys + 0xFFFFFFFF80000000ULL);
    uint64_t *pd   = (uint64_t*)(uintptr_t)(pd_phys   + 0xFFFFFFFF80000000ULL);

    for (int i=0;i<512;i++){ pml4[i]=0; pdpt[i]=0; pd[i]=0; }

    pml4[0] = pdpt_phys | PTE_PRESENT | PTE_RW;
    pdpt[0] = pd_phys   | PTE_PRESENT | PTE_RW;

    uint64_t flags = PTE_PRESENT | PTE_RW | PTE_PS;
    if (!exec) flags |= PTE_NX;

    uint64_t paddr = phys_base;
    for (uint64_t i=0;i<pages2m && i<512;i++){
        pd[i] = (paddr & ~(PAGE_2M-1)) | flags;
        paddr += PAGE_2M;
    }

    load_cr3(pml4_phys);
    return pml4_phys;
}