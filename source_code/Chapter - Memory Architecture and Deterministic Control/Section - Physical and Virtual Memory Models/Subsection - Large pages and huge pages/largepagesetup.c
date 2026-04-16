#include 
#include 

/* Page table pages must be 4KiB-aligned */
static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd  [512] __attribute__((aligned(4096)));

/* Basic flags: Present | RW | User (modify as needed) */
#define PTE_PRESENT  (1ULL<<0)
#define PTE_RW       (1ULL<<1)
#define PTE_USER     (1ULL<<2)
#define PTE_PWT      (1ULL<<3)
#define PTE_PCD      (1ULL<<4)
#define PTE_ACCESSED (1ULL<<5)
#define PTE_DIRTY    (1ULL<<6)
#define PTE_PS       (1ULL<<7) /* Page Size for PDE/PDPT */
#define PTE_GLOBAL   (1ULL<<8)
#define PTE_NX       (1ULL<<63)

/* Should be physical addresses in a bare-metal boot loader */
void build_identity_map_1GiB_2MiB(void) {
    for (size_t i = 0; i < 512; ++i) { pml4[i] = 0; pdpt[i] = 0; pd[i] = 0; }

    /* Link tables: assume &pd is identity physical for this example. */
    pml4[0]  = ((uint64_t)pdpt) | PTE_PRESENT | PTE_RW;
    pdpt[0]  = ((uint64_t)pd)   | PTE_PRESENT | PTE_RW;

    /* Populate PD with 512 PDEs, each mapping 2MiB using PS bit */
    const uint64_t two_mib = 2ULL * 1024 * 1024;
    for (size_t i = 0; i < 512; ++i) {
        uint64_t phys = (uint64_t)i * two_mib;
        pd[i] = phys | PTE_PRESENT | PTE_RW | PTE_PS;
    }

    /* After this, load CR3 with physical address of pml4 (platform-specific) */
    /* Example assembly (requires identity phys ptr): mov cr3, rax where rax = pml4 */
}
/* Caller must ensure the arrays reside at identity-mapped physical addresses,
   or translate virtual->physical before writing CR3. Also, set NX flags as needed. */