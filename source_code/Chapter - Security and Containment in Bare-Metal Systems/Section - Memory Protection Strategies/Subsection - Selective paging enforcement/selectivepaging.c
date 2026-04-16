#include 
#include 
#include 
#include 

/* PTE flags */
enum { PTE_P = 1ULL<<0, PTE_RW = 1ULL<<1, PTE_US = 1ULL<<2,
       PTE_PWT = 1ULL<<3, PTE_PCD = 1ULL<<4, PTE_A = 1ULL<<5,
       PTE_D = 1ULL<<6, PTE_PS = 1ULL<<7, PTE_G = 1ULL<<8,
       PTE_NX = 1ULL<<63 };

/* Aligned page-table structures (512 entries each) */
alignas(4096) static uint64_t pml4[512];
alignas(4096) static uint64_t pdpt[512];
alignas(4096) static uint64_t pd_kernel[512]; /* will hold 2MiB mappings */
alignas(4096) static uint64_t pt_prot[512];   /* optional 4KiB mappings */

/* Map low 1 GiB with 2MiB pages (deterministic region). */
static void setup_kernel_identity(void) {
    memset(pml4, 0, sizeof(pml4));
    memset(pdpt, 0, sizeof(pdpt));
    memset(pd_kernel, 0, sizeof(pd_kernel));
    /* PML4 -> PDPT */
    pml4[0] = ((uint64_t)pdpt) | PTE_P | PTE_RW | PTE_G;
    /* PDPT -> PD (use first PD entry to cover 1 GiB with 2MiB PDEs) */
    pdpt[0] = ((uint64_t)pd_kernel) | PTE_P | PTE_RW | PTE_G;
    /* Fill PD with 2MiB pages for first 1 GiB (512 entries * 2MiB = 1GiB) */
    for (size_t i = 0; i < 512; ++i) {
        uint64_t phys = (uint64_t)i * 0x200000ULL; /* 2MiB step */
        pd_kernel[i] = phys | PTE_P | PTE_RW | PTE_PS | PTE_G;
    }
    /* Protected region left unmapped: do not populate other PDPT entries */
}

/* Load CR3 with the physical address of PML4. Caller must be ring 0. */
static inline void load_cr3(void *pml4_phys) {
    asm volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

/* Example usage: setup and activate tables. */
void enable_selective_paging(void) {
    setup_kernel_identity();
    load_cr3(pml4); /* assumes physical==virtual identity for this address */
    /* Now protected regions are non-present; install page-fault handler. */
}