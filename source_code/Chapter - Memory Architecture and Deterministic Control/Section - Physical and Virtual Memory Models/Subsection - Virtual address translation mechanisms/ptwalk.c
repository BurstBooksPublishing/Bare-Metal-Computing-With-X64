#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#define PTE_PRESENT  (1ULL<<0)
#define PTE_RW       (1ULL<<1)
#define PTE_USER     (1ULL<<2)
#define PTE_PWT      (1ULL<<3)
#define PTE_PCD      (1ULL<<4)
#define PTE_ACCESSED (1ULL<<5)
#define PTE_DIRTY    (1ULL<<6)
#define PTE_PS       (1ULL<<7)
#define PTE_NX       (1ULL<<63)

/* phys_map_base: pointer where physical memory address 0 is mapped.
   cr3: value of CR3 register (physical base of PML4, low bits ignored).
   va: virtual address to translate.
   Returns physical address or UINT64_MAX on fault. */
static uint64_t translate_va(uint64_t cr3, uint64_t va, volatile void *phys_map_base) {
    volatile uint8_t *phys = (volatile uint8_t *)phys_map_base;
    uint64_t frame_mask = 0x000ffffffffff000ULL;
    uint64_t addr;
    uint64_t entry;
    uint64_t indices[4];

    /* Extract indices per Eq. (1) */
    indices[0] = (va >> 12) & 0x1FF;        /* PT */
    indices[1] = (va >> 21) & 0x1FF;        /* PD */
    indices[2] = (va >> 30) & 0x1FF;        /* PDPT */
    indices[3] = (va >> 39) & 0x1FF;        /* PML4 */

    /* Normalize CR3 to physical base (bits 51:12) */
    uint64_t table_phys = cr3 & frame_mask;

    for (int level = 3; level >= 0; --level) {
        /* compute entry physical address: table_phys + 8 * index */
        addr = table_phys + (indices[level] * sizeof(uint64_t));
        entry = *(volatile uint64_t *)(phys + addr); /* read PTE from physical window */

        if (!(entry & PTE_PRESENT)) return UINT64_MAX; /* page fault: not present */

        /* If PS bit set in PDPT (level==2) -> 1GiB page */
        if (level == 2 && (entry & PTE_PS)) {
            uint64_t base = entry & 0x000fffffc0000000ULL; /* bits 51:30 << 30 */
            uint64_t page_offset = va & 0x3FFFFFFFULL;     /* low 30 bits */
            return base | page_offset;
        }
        /* If PS bit set in PD (level==1) -> 2MiB page */
        if (level == 1 && (entry & PTE_PS)) {
            uint64_t base = entry & 0x000fffffffe00000ULL; /* bits 51:21 << 21 */
            uint64_t page_offset = va & 0x1FFFFFULL;       /* low 21 bits */
            return base | page_offset;
        }

        /* descend: next table base from entry frame bits */
        table_phys = entry & frame_mask;
    }

    /* level==0: final PTE for 4KiB page */
    uint64_t phys_base = entry & frame_mask;
    uint64_t offset = va & 0xFFF;
    return phys_base | offset;
}