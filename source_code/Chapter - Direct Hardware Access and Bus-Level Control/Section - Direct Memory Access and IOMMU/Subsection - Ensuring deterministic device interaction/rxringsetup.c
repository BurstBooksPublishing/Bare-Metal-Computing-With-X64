#include 
#include 

#define RX_RING_ENTRIES 1024
#define RX_PKT_SIZE 2048
#define PAGE_SIZE 4096

// Hardware helpers (platform-specific, implemented elsewhere).
extern void *alloc_phys_contiguous(size_t bytes); // returns physical address
extern void *phys_to_virt(uint64_t phys);        // maps phys for CPU access
extern int iommu_map_range(uint64_t dev_sid, uint64_t phys, size_t size, int perms);
volatile uint32_t *nic_mmio_base;                 // mapped device BAR

static inline void clwb_sfence(void *addr, size_t bytes) {
    // Flush cache lines covering [addr, addr+bytes)
    uintptr_t p = (uintptr_t)addr;
    uintptr_t end = p + bytes;
    for (; p < end; p += 64) {
        asm volatile("clwb (%0)" :: "r"((void*)p) : "memory");
    }
    asm volatile("sfence" ::: "memory"); // ensure writeback visibility
}

int setup_rx_ring(uint64_t dev_sid) {
    size_t ring_bytes = RX_RING_ENTRIES * sizeof(uint64_t); // descriptor entries
    size_t payload_bytes = RX_RING_ENTRIES * RX_PKT_SIZE;
    size_t total = (ring_bytes + payload_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    uint64_t phys = (uint64_t)alloc_phys_contiguous(total);
    if (!phys) return -1;

    // Map for CPU access.
    void *base = phys_to_virt(phys);
    // Initialize descriptors in-place.
    uint64_t *desc = (uint64_t *)base;
    for (size_t i = 0; i < RX_RING_ENTRIES; ++i) {
        uint64_t pkt_phys = phys + ring_bytes + i * RX_PKT_SIZE;
        desc[i] = pkt_phys; // device expects physical pointer in descriptor
    }

    // Map range in IOMMU with device SID; pin mapping to avoid faults.
    if (iommu_map_range(dev_sid, phys, total, /*perms=*/0x3) != 0) return -2;

    // Ensure all descriptor writes are visible to device before doorbell.
    clwb_sfence(base, ring_bytes);

    // Program NIC registers: set RX ring base and length, then enable.
    volatile uint32_t *mmio = nic_mmio_base;
    mmio[0x100/4] = (uint32_t)(phys & 0xffffffff);         // RX base low
    mmio[0x104/4] = (uint32_t)((phys >> 32) & 0xffffffff); // RX base high
    mmio[0x108/4] = RX_RING_ENTRIES;                       // RX length
    mmio[0x10C/4] = 1;                                     // RX enable

    return 0;
}