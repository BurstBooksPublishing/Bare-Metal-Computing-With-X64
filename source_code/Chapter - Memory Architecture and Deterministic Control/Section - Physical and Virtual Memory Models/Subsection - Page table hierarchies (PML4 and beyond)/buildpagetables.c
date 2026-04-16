/* Bare-metal page table builder: maps [0, map_size) identity
   and maps same physical region at KERNEL_BASE using 2MiB pages. */
#include 
#include 
#include 

#define PAGE_ENTRIES 512
#define PAGE_SIZE 4096ULL
#define LARGE_PAGE_SIZE (2ULL*1024*1024)

#define PTE_PRESENT   (1ULL<<0)
#define PTE_RW        (1ULL<<1)
#define PTE_USER      (1ULL<<2)
#define PTE_PWT       (1ULL<<3)
#define PTE_PCD       (1ULL<<4)
#define PTE_ACCESSED  (1ULL<<5)
#define PTE_DIRTY     (1ULL<<6)
#define PTE_PS        (1ULL<<7)  /* when set in PD -> 2MiB */
#define PTE_GLOBAL    (1ULL<<8)
#define PTE_NX        (1ULL<<63)

#define KERNEL_BASE 0xFFFF800000000000ULL

/* Aligned storage for one PML4, one PDPT, and PAGE_ENTRIES PD pages. */
static uint64_t pml4[PAGE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t pdpt[PAGE_ENTRIES] __attribute__((aligned(4096)));
static uint64_t pd_table[PAGE_ENTRIES][PAGE_ENTRIES] __attribute__((aligned(4096)));

static inline uint64_t phys_of(void *p) { return (uint64_t)(uintptr_t)p; }

/* Map [phys,start_phys+size) using 2MiB pages into both identity and higher-half. */
void build_mappings(uint64_t size)
{
    assert((size % LARGE_PAGE_SIZE) == 0);
    /* PML4 entry 0 for identity, entry 511 for higher-half (canonical top). */
    pml4[0] = phys_of(pdpt) | PTE_PRESENT | PTE_RW;
    pml4[511] = phys_of(pdpt) | PTE_PRESENT | PTE_RW; /* reuse same PDPT for simplicity */

    /* For each PDPT entry, install a PD base. */
    for (size_t pdpt_i = 0; pdpt_i < PAGE_ENTRIES; ++pdpt_i) {
        pdpt[pdpt_i] = phys_of(pd_table[pdpt_i]) | PTE_PRESENT | PTE_RW;
    }

    /* Populate PD entries with 2MiB pages. */
    uint64_t pages = size / LARGE_PAGE_SIZE;
    uint64_t mapped = 0;
    for (uint64_t pdpt_i = 0; pdpt_i < PAGE_ENTRIES && mapped < pages; ++pdpt_i) {
        for (uint64_t pd_i = 0; pd_i < PAGE_ENTRIES && mapped < pages; ++pd_i, ++mapped) {
            uint64_t phys_frame = mapped * LARGE_PAGE_SIZE;
            uint64_t entry = (phys_frame & 0xFFFFFFFFFFE00000ULL) /* bits 51:21 */
                              | PTE_PRESENT | PTE_RW | PTE_PS;
            pd_table[pdpt_i][pd_i] = entry;
        }
    }
}

/* Load CR3 with physical address of pml4; ensure TLB shootdown not needed at boot. */
static inline void load_cr3(uint64_t phys_pml4)
{
    __asm__ volatile("mov %0, %%cr3" :: "r"(phys_pml4) : "memory");
}

/* Usage from boot code:
   build_mappings(1ULL<<30); // map 1 GiB
   load_cr3(phys_of(pml4));
*/