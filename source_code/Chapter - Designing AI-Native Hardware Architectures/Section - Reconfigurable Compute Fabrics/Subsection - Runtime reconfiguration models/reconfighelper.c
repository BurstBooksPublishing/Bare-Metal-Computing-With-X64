#include 
#include 

#define MMIO_REG_CTRL ((volatile uint32_t*)0xF0000000) // device control
#define MMIO_REG_STATUS ((volatile uint32_t*)0xF0000004) // device status
#define DMA_PR_ADDR_REG ((volatile uint64_t*)0xF0000010) // PR buffer addr
#define DMA_PR_LEN_REG  ((volatile uint32_t*)0xF0000018) // PR len
#define DMA_START_REG   ((volatile uint32_t*)0xF000001C) // start DMA

// Flush host memory range so device DMA sees latest data.
static void flush_host_range(void *addr, size_t len) {
    uintptr_t p = (uintptr_t)addr;
    const size_t CL = 64;
    uintptr_t end = (p + len + CL - 1) & ~(CL - 1);
    for (; p < end; p += CL) {
        asm volatile("clwb (%0)" :: "r"((void*)p) : "memory"); // write-back cacheline
    }
    asm volatile("sfence" ::: "memory"); // ensure ordering
}

// Returns 0 on success, negative on error.
int reconfigure_pr_region(void *bitstream, size_t len, uint32_t timeout_ms) {
    // 1) quiesce device
    *MMIO_REG_CTRL = 0x0; // clear run bit (device-dependent)
    // 2) ensure all host buffers flushed
    flush_host_range(bitstream, len);
    // 3) program DMA to copy bitstream into device PR buffer
    *DMA_PR_ADDR_REG = (uint64_t)(uintptr_t)bitstream;
    *DMA_PR_LEN_REG = (uint32_t)len;
    *DMA_START_REG = 1; // start DMA
    // 4) trigger device PR after DMA completes (device guarantees DMA done)
    *MMIO_REG_CTRL = 0x10; // trigger PR (device-defined)
    // 5) poll status
    uint32_t elapsed = 0;
    while (!(*MMIO_REG_STATUS & 0x1)) { // PR done bit
        // small busy-wait; in bare-metal, use calibrated delay
        if (elapsed++ > timeout_ms) return -1; // timeout
    }
    // 6) verify CRC and commit
    if (!(*MMIO_REG_STATUS & 0x2)) return -2; // CRC failed
    *MMIO_REG_CTRL = 0x1; // set run bit
    return 0;
}