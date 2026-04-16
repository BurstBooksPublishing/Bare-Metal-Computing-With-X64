#include 
#include 

// MMIO base for FPGA control BAR (platform-specific).
#define FPGA_CTL_BASE 0xF8000000UL
#define REG_CTRL      0x00
#define REG_STATUS    0x04
#define REG_PR_CMD    0x10
#define REG_PR_STAT   0x14
#define REG_PR_PTR_LO 0x18
#define REG_PR_PTR_HI 0x1C
#define STATUS_IDLE   0x1
#define PR_DONE       0x1

static inline void mmio_write32(uintptr_t base, uint32_t off, uint32_t v) {
    volatile uint32_t *p = (volatile uint32_t *)(base + off);
    *p = v;
    __asm__ volatile("sfence" ::: "memory"); // order writes
}
static inline uint32_t mmio_read32(uintptr_t base, uint32_t off) {
    volatile uint32_t *p = (volatile uint32_t *)(base + off);
    uint32_t v = *p;
    __asm__ volatile("lfence" ::: "memory");
    return v;
}

// Busy-wait with rdtsc-based timeout (microseconds).
static uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static bool wait_status(uintptr_t base, uint32_t off, uint32_t mask,
                        uint32_t expected, uint64_t timeout_us, uint64_t cpu_freq_hz) {
    uint64_t start = rdtsc();
    uint64_t deadline = start + (timeout_us * cpu_freq_hz) / 1000000ULL;
    while (rdtsc() < deadline) {
        if ((mmio_read32(base, off) & mask) == expected) return true;
    }
    return false;
}

// Flush cache lines for physical memory region [paddr, paddr+len).
static void clflush_range(void *p, size_t len) {
    uintptr_t a = (uintptr_t)p & ~((uintptr_t)63);
    uintptr_t end = (uintptr_t)p + len;
    for (; a < end; a += 64) __asm__ volatile("clflushopt (%0)" :: "r"(a));
    __asm__ volatile("sfence" ::: "memory");
}

// High-level orchestration (assumes IOMMU and page-table APIs exist).
bool partial_reconfigure(uint64_t bitstream_paddr_lo, uint64_t bitstream_paddr_hi,
                         uint64_t cpu_freq_hz) {
    // 1) Request device quiesce.
    mmio_write32(FPGA_CTL_BASE, REG_CTRL, 0x2); // set QUIESCE bit
    if (!wait_status(FPGA_CTL_BASE, REG_STATUS, STATUS_IDLE, STATUS_IDLE, 100000, cpu_freq_hz))
        return false; // device didn't idle

    // 2) Revoke IOMMU mappings into region R (platform-specific).
    //    This must ensure (\ref{eq:dma_safety}). Call platform IOMMU API here.
    // iommu_revoke_region(R_base, R_len);

    // 3) Flush caches for communication pages (example pointer).
    extern void *comm_buffer;
    extern size_t comm_buffer_len;
    clflush_range(comm_buffer, comm_buffer_len);

    // 4) Invalidate TLBs: reload CR3 (simple and robust).
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");

    // 5) Program bitstream pointer (low/high 32-bit).
    mmio_write32(FPGA_CTL_BASE, REG_PR_PTR_LO, (uint32_t)bitstream_paddr_lo);
    mmio_write32(FPGA_CTL_BASE, REG_PR_PTR_HI, (uint32_t)bitstream_paddr_hi);

    // 6) Trigger PR and wait for completion.
    mmio_write32(FPGA_CTL_BASE, REG_PR_CMD, 0x1); // start PR
    if (!wait_status(FPGA_CTL_BASE, REG_PR_STAT, PR_DONE, PR_DONE, 5000000, cpu_freq_hz)) {
        // on failure, attempt rollback (device-specific) and return error.
        mmio_write32(FPGA_CTL_BASE, REG_PR_CMD, 0x4); // rollback request
        return false;
    }

    // 7) Restore IOMMU mappings and unquiesce.
    // iommu_restore_region(...);
    mmio_write32(FPGA_CTL_BASE, REG_CTRL, 0x0); // clear QUIESCE
    return true;
}