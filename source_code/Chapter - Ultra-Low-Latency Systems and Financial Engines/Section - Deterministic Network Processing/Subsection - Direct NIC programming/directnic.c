/* Production-grade: check return values, handle 64-bit BARs, and consult NIC datasheet for register offsets. */
#include 
#include 

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Read 32-bit PCI config at bus/dev/fn/offset using CF8/CFC legacy mechanism. */
uint32_t pci_cfg_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t offset) {
    uint32_t addr = 0x80000000u
        | ((uint32_t)bus << 16)
        | ((uint32_t)dev << 11)
        | ((uint32_t)fn  << 8)
        | (offset & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

/* Assume an identity-mapped physical allocator exists: alloc_phys_pages(count, align). */
/* Structures chosen are generic; adapt per NIC descriptor format. */
#define RX_RING_SIZE 1024      /* power of two */
#define RX_BUF_SIZE  2048
struct rx_desc { uint64_t buf_phys; uint16_t len; uint16_t flags; uint32_t reserved; } __attribute__((packed));

/* Example routine: map BAR0 and initialize RX ring. */
void nic_init_rx(uint8_t bus, uint8_t dev, uint8_t fn, volatile void **mmio_base_out) {
    uint32_t bar0_lo = pci_cfg_read32(bus, dev, fn, 0x10);
    uint64_t bar0 = bar0_lo & ~0xFu; /* assume 32-bit BAR for simplicity; check type in production */
    volatile uint8_t *mmio = (volatile uint8_t *)(uintptr_t)bar0;
    *mmio_base_out = mmio;

    /* Allocate RX ring region: contiguous DMA physical memory aligned to 4096. */
    size_t ring_bytes = RX_RING_SIZE * sizeof(struct rx_desc);
    uintptr_t ring_phys = alloc_phys_pages((ring_bytes + 4095)/4096, 4096); /* identity mapped */
    struct rx_desc *rx_ring = (struct rx_desc *)ring_phys;

    /* Allocate packet buffers and attach. */
    for (size_t i = 0; i < RX_RING_SIZE; ++i) {
        uintptr_t buf_phys = alloc_phys_pages((RX_BUF_SIZE + 4095)/4096, 4096); /* page-aligned buffer */
        rx_ring[i].buf_phys = (uint64_t)buf_phys;
        rx_ring[i].len = 0;
        rx_ring[i].flags = 0; /* device-specific ownership bit cleared */
    }

    /* Ensure memory writes complete before exposing to device. */
    __asm__ volatile ("sfence" ::: "memory");

    /* Write the device's RX ring base registers (register offsets are NIC-specific). */
    /* Example offsets: RX_DESC_BASE_LOW = 0x2800, RX_DESC_BASE_HIGH = 0x2804, RX_TAIL = 0x2818 */
    volatile uint32_t *rx_desc_base_low  = (volatile uint32_t *)(mmio + 0x2800);
    volatile uint32_t *rx_desc_base_high = (volatile uint32_t *)(mmio + 0x2804);
    volatile uint32_t *rx_tail           = (volatile uint32_t *)(mmio + 0x2818);

    *rx_desc_base_low  = (uint32_t)(ring_phys & 0xFFFFFFFF);
    *rx_desc_base_high = (uint32_t)(ring_phys >> 32);

    /* Initially head is 0; set tail to RX_RING_SIZE-1 to indicate populated ring (device-specific convention). */
    *rx_tail = RX_RING_SIZE - 1;
}