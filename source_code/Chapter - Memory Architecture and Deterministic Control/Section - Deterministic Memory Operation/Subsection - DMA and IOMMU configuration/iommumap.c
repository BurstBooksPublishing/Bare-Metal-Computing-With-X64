#include 
#include 

/* Bare-metal MMIO helpers (volatile accesses). */
static inline void mmio_write64(uintptr_t base, uint64_t offset, uint64_t val) {
    volatile uint64_t *reg = (volatile uint64_t *)(base + offset);
    *reg = val;
}
static inline uint64_t mmio_read64(uintptr_t base, uint64_t offset) {
    volatile uint64_t *reg = (volatile uint64_t *)(base + offset);
    return *reg;
}

/* Platform-specific offsets per Intel VT-d spec (common placements). */
#define DMAR_CAP      0x000ULL
#define DMAR_ECAP     0x010ULL
#define DMAR_GCMD     0x018ULL
#define DMAR_GSTS     0x01CULL
#define DMAR_RTADDR   0x020ULL

/* Bits (implementation-defined per spec - verify on target). */
#define GCMD_TE       (1ULL << 31) /* Translation Enable (VT-d spec) */
#define GCMD_SRTP     (1ULL << 30) /* Set Root Table Pointer (write-1-to-set) */

extern void *alloc_phys_pages(size_t pages, size_t align_pages); /* platform allocator */
extern uintptr_t phys_addr(void *virt); /* identity/phys mapping helper */

/* Flush cache lines for buffer range before device reads. */
static void flush_buffer_for_device(void *buf, size_t len) {
    uintptr_t p = (uintptr_t)buf;
    for (uintptr_t addr = p & ~0x3FULL; addr < p + len; addr += 64) {
        __asm__ volatile ("clflushopt (%0)" :: "r" (addr));
    }
    __asm__ volatile ("sfence" ::: "memory");
}

/* Map a single 2MiB buffer for device B:D:F. */
int iommu_map_2MiB(uintptr_t iommu_base, uint8_t bus, uint8_t dev, uint8_t fn,
                   void **dma_buf_out, uintptr_t iova_base) {
    const size_t P2M = 2 * 1024 * 1024;
    void *buf = alloc_phys_pages(P2M / 4096, P2M / 4096); /* allocate contiguous 2MiB */
    if (!buf) return -1;
    uintptr_t phys = phys_addr(buf);

    /* Allocate domain page-table root (one 4K page), and one context table page. */
    void *domain_root = alloc_phys_pages(1,1);
    void *ctx_table = alloc_phys_pages(1,1);
    void *root_table = alloc_phys_pages(1,1);

    /* Build a single 2MiB (large page) PDE in domain root mapping IOVA->phys. */
    uint64_t *droot = (uint64_t*)domain_root;
    uint64_t pde = (phys & 0xFFFFFFFFFFE00000ULL)   /* physical base aligned to 2MiB */
                 | (1ULL << 7)  /* large page flag in VT-d second-level format */
                 | 0x1ULL;       /* present/write */
    droot[(iova_base >> 21) & 0x1FF] = pde; /* index into level covering 2MiB pages */

    /* Context entry: set domain ID 1 and point to domain_root. Context table entry format per spec. */
    uint64_t *ctx = (uint64_t*)ctx_table;
    uint64_t domain_id = 1;
    uint64_t ctx_entry_low = (domain_id & 0xFFFFULL) | (0ULL << 16); /* flags zeroed */
    uint64_t ctx_entry_high = (phys_addr(domain_root) & 0xFFFFFFFFFFFFF000ULL);
    ctx[(dev << 3) | fn] = ctx_entry_low;
    ctx[((dev << 3) | fn) + 1] = ctx_entry_high;

    /* Root entry: point bus slot to ctx_table. */
    uint64_t *rt = (uint64_t*)root_table;
    rt[bus] = (phys_addr(ctx_table) & 0xFFFFFFFFFFFFF000ULL) | 0x1ULL;

    /* Program RT address and enable translations. */
    mmio_write64(iommu_base, DMAR_RTADDR, phys_addr(root_table));
    mmio_write64(iommu_base, DMAR_GCMD, GCMD_SRTP); /* set root pointer */
    mmio_write64(iommu_base, DMAR_GCMD, GCMD_TE);   /* enable translation */

    /* Ensure buffer visible to device. */
    flush_buffer_for_device(buf, P2M);

    *dma_buf_out = buf;
    return 0;
}