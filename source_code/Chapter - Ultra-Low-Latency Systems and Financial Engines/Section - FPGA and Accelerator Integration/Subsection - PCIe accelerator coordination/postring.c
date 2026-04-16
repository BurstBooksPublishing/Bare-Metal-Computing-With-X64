#include 
#include 

static inline void clwb(void *addr) {
    // Flush a single cache line with CLWB; compiler barrier prevents reordering.
    asm volatile("clwb (%0)" :: "r"(addr) : "memory");
}

static inline void sfence_barrier(void) {
    asm volatile("sfence" ::: "memory");
}

void post_descriptor_and_ring(volatile void *bar_base,
                              uintptr_t doorbell_offset,
                              void *desc_ptr,
                              size_t desc_bytes,
                              uint32_t doorbell_value)
{
    // 1) Build descriptor in memory (caller writes fields before calling).
    // 2) Flush affected cache lines so DMA engine will see descriptor.
    const size_t CL = 64; // cache line size
    uintptr_t p = (uintptr_t)desc_ptr;
    uintptr_t end = p + desc_bytes;
    // Align down to CL for first flush.
    p &= ~(CL - 1);
    for (; p < end; p += CL) clwb((void *)p);

    // 3) Ensure persistence ordering to device.
    sfence_barrier();

    // 4) Ring the doorbell via MMIO write to BAR (posted write).
    volatile uint32_t *doorbell = (volatile uint32_t *)((uintptr_t)bar_base + doorbell_offset);
    *doorbell = doorbell_value; // initiates work on device

    // 5) Optionally do a read-back for strong ordering if needed.
    (void)*doorbell;
}