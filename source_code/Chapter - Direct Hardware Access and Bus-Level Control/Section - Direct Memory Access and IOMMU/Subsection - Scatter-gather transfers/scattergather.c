#include 
#include 
#include 

#define PAGE_SIZE 4096
#define MAX_SG_ENTRIES 256

/* Hardware descriptor format for a generic device */
struct sg_desc {
    uint64_t dma_addr; /* IOVA visible to device */
    uint32_t length;
    uint32_t flags;
};

/* Abstract: map a physical page range to an IOVA and return device-visible base.
   Implement with platform VT-d/AMD-Vi AP table updates. */
extern int iommu_map_iova(uint64_t iova_base, uint64_t phys_base, size_t size, int prot);

/* Flush range to memory using CLFLUSHOPT, then SFENCE. */
static inline void clflush_range(void *addr, size_t len)
{
    uintptr_t p = (uintptr_t)addr;
    uintptr_t end = p + len;
    for (; p < end; p += 64) {
        __asm__ volatile("clflushopt (%0)" :: "r"(p) : "memory");
    }
    __asm__ volatile("sfence" ::: "memory");
}

/* Build SG list by splitting on page boundary and mapping pages into IOVA. */
int build_sg_list(void *bufs[], size_t buflen[], int nbuf,
                  struct sg_desc out[], int max_out, uint64_t iova_base_start)
{
    if (nbuf <= 0) return -EINVAL;
    int out_idx = 0;
    uint64_t iova = iova_base_start;

    for (int i = 0; i < nbuf; ++i) {
        uint8_t *v = (uint8_t *)bufs[i];
        size_t remaining = buflen[i];
        /* Flush cpu-writes so device reads coherent data. */
        clflush_range(v, remaining);

        while (remaining) {
            if (out_idx >= max_out) return -ENOSPC;
            uintptr_t page_off = (uintptr_t)v & (PAGE_SIZE - 1);
            size_t chunk = PAGE_SIZE - page_off;
            if (chunk > remaining) chunk = remaining;

            uint64_t phys = (uint64_t)/* platform function */(v); /* replace with virt->phys */
            /* map one page-aligned region to IOVA */
            int rc = iommu_map_iova(iova, phys - page_off, chunk + page_off, /*prot=*/0x3);
            if (rc) return rc;

            out[out_idx].dma_addr = iova + page_off;
            out[out_idx].length = (uint32_t)chunk;
            out[out_idx].flags = 0;
            out_idx++;

            iova += PAGE_SIZE; /* advance by page granularity */
            v += chunk;
            remaining -= chunk;
        }
    }
    return out_idx; /* number of descriptors emitted */
}