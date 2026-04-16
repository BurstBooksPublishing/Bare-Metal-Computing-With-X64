#include 
#include 
#include 
#include 

/* Simple physical frame allocator for page-aligned pages. */
#define PAGE_SIZE 4096
#define POOL_PAGES 4096
static alignas(PAGE_SIZE) uint8_t frame_pool[POOL_PAGES * PAGE_SIZE];
static size_t frame_next = 0;

static void *alloc_page_phys(void) {
    if (frame_next >= POOL_PAGES) return NULL;
    void *p = &frame_pool[frame_next * PAGE_SIZE];
    frame_next++;
    memset(p, 0, PAGE_SIZE);
    return p;
}

/* PTE flags for an IOMMU: present | read/write | snoop (platform dependent). */
enum { IOMMU_PTE_PRESENT = 1ULL<<0, IOMMU_PTE_WRITE = 1ULL<<1, IOMMU_PTE_SNOOP = 1ULL<<2 };

/* Writes to hardware root/context entries - platform must implement. */
extern void iommu_write_root_entry(uint8_t bus, uint64_t root_entry_phys);
extern void iommu_write_context_entry(uint8_t bus, uint8_t dev, uint8_t func, uint64_t ctx_entry_low, uint64_t ctx_entry_high);
extern void iommu_flush_iotlb(void); /* Platform-specific IOTLB flush. */

/* Build a simple 4-level page table mapping IOVA->phys for size bytes.
   Returns the physical address of the top-level table. */
uint64_t build_iommu_tables(uint64_t iova_base, uint64_t phys_base, size_t size, uint64_t flags) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    /* allocate PML4, PDPT, PD, PT arrays on demand */
    uint64_t *pml4 = alloc_page_phys();
    if (!pml4) return 0;
    uint64_t pml4_idx = (iova_base >> 39) & 0x1FF;
    uint64_t *pdpt = alloc_page_phys();
    pml4[pml4_idx] = ((uint64_t)pdpt) | IOMMU_PTE_PRESENT;
    uint64_t pdpt_idx = (iova_base >> 30) & 0x1FF;
    uint64_t *pd = alloc_page_phys();
    pdpt[pdpt_idx] = ((uint64_t)pd) | IOMMU_PTE_PRESENT;
    uint64_t pd_idx = (iova_base >> 21) & 0x1FF;
    uint64_t *pt = alloc_page_phys();
    pd[pd_idx] = ((uint64_t)pt) | IOMMU_PTE_PRESENT;
    uint64_t pt_idx = (iova_base >> 12) & 0x1FF;
    uint64_t off = 0;
    for (size_t i = 0; i < pages; ++i) {
        uint64_t paddr = phys_base + off;
        pt[pt_idx] = (paddr & ~0xFFFULL) | (flags & 0xFFFULL);
        pt_idx++;
        if (pt_idx == 512) {
            pt = alloc_page_phys();
            pd_idx++;
            pd[pd_idx] = ((uint64_t)pt) | IOMMU_PTE_PRESENT;
            pt_idx = 0;
        }
        off += PAGE_SIZE;
    }
    return (uint64_t)pml4;
}

/* High-level bind: create domain, map IOVA->phys, and install PCI context entry. */
int bind_pci_device_to_domain(uint8_t bus, uint8_t dev, uint8_t func,
                              uint64_t iova_base, uint64_t phys_base, size_t size, uint16_t domain_id) {
    uint64_t flags = IOMMU_PTE_PRESENT | IOMMU_PTE_WRITE | IOMMU_PTE_SNOOP;
    uint64_t pml4_phys = build_iommu_tables(iova_base, phys_base, size, flags);
    if (!pml4_phys) return -1;
    /* Context entry low: domain ID in low bits; high: pointer to root of domain tables */
    uint64_t ctx_low = (uint64_t)domain_id & 0xFFFF;
    uint64_t ctx_high = (pml4_phys & ~0xFFFULL);
    /* Platform must write root entry for bus to point to context table; assume context table allocated elsewhere */
    iommu_write_context_entry(bus, dev, func, ctx_low, ctx_high);
    iommu_flush_iotlb();
    return 0;
}