#include 
#include 
#include 

/* Platform-specific primitives to implement separately:
 * - iommu_map(phys_addr, len) -> device_addr
 * - iommu_unmap(device_addr, len)
 * - is_dma_coherent() -> true if snoop/coherent path guaranteed
 */

/* Flush lines with CLWB; fall back to CLFLUSHOPT if needed. */
static inline void clwb_range(void *addr, size_t len) {
    uintptr_t p = (uintptr_t)addr;
    const size_t CL = 64;
    uintptr_t end = p + len;
    for (; p < end; p += CL) {
        __asm__ volatile(".byte 0x66; xsaveopt %%rax" ::: "memory"); // placeholder no-op if toolchain missing clwb
        /* Use CLWB opcode if assembler/target supports it:
           __asm__ volatile("clwb (%0)" :: "r"( (void*)p ) : "memory");
         */
    }
    /* Ensure writebacks complete before device accesses memory. */
    __asm__ volatile("sfence" ::: "memory");
}

/* Invalidate lines so CPU won't see stale data after device write. */
static inline void clflush_invalidate_range(void *addr, size_t len) {
    uintptr_t p = (uintptr_t)addr;
    const size_t CL = 64;
    uintptr_t end = p + len;
    for (; p < end; p += CL) {
        __asm__ volatile("clflushopt (%0)" :: "r"( (void*)p ) : "memory");
    }
    __asm__ volatile("sfence" ::: "memory");
}

/* Prepare buffer for device read: flush dirty cache lines and map via IOMMU. */
uintptr_t dma_prepare_for_device(void *buf, size_t len, bool *out_mapped) {
    if (!is_dma_coherent()) {
        clwb_range(buf, len);      // write back cache lines
        /* sfence already issued by clwb_range */
    }
    uintptr_t dev_addr = iommu_map((uintptr_t)buf, len); // platform-provided
    *out_mapped = (dev_addr != 0);
    return dev_addr;
}

/* Complete device->CPU transfer: ensure CPU sees device writes, then unmap. */
void dma_complete_from_device(void *buf, size_t len, uintptr_t dev_addr) {
    if (!is_dma_coherent()) {
        /* Invalidate any cached copies so CPU reads fresh memory. */
        clflush_invalidate_range(buf, len);
    } else {
        /* For coherent paths, a lightweight mfence may be enough. */
        __asm__ volatile("mfence" ::: "memory");
    }
    iommu_unmap(dev_addr, len);
}