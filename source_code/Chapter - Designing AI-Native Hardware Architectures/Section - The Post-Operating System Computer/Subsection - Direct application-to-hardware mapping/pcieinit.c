#include 
#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// Port I/O primitives (require ring0 privileges).
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %w1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    asm volatile ("inl %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// Build PCI config address for bus/dev/func/offset.
static inline uint32_t pci_cfg_addr(uint8_t bus, uint8_t dev,
                                    uint8_t func, uint8_t off) {
    return 0x80000000u | ((uint32_t)bus << 16) |
           ((uint32_t)dev << 11) | ((uint32_t)func << 8) | (off & 0xFC);
}

// Read 32-bit config register.
static uint32_t pci_read_cfg(uint8_t bus, uint8_t dev,
                             uint8_t func, uint8_t off) {
    outl(PCI_CONFIG_ADDR, pci_cfg_addr(bus, dev, func, off));
    return inl(PCI_CONFIG_DATA);
}

// Simple linear scan to find a device by vendor/device ID.
int find_pci_device(uint16_t vendor_id, uint16_t device_id,
                    uint8_t *out_bus, uint8_t *out_dev, uint8_t *out_func) {
    for (uint8_t bus = 0; bus < 256; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            for (uint8_t func = 0; func < 8; ++func) {
                uint32_t id = pci_read_cfg(bus, dev, func, 0x00);
                if ((uint16_t)id == 0xFFFF) continue; // no device
                if ((uint16_t)id == vendor_id &&
                    (uint16_t)(id >> 16) == device_id) {
                    *out_bus = bus; *out_dev = dev; *out_func = func;
                    return 0;
                }
            }
        }
    }
    return -1;
}

// Minimal descriptor format; device must be compatible.
struct dma_desc {
    uint64_t addr;   // physical address of buffer
    uint32_t len;    // length in bytes
    uint32_t flags;  // consumer-specific flags
} __attribute__((packed, aligned(16)));

void init_device_and_ring(uint8_t bus, uint8_t dev, uint8_t func,
                          struct dma_desc *ring, uint32_t ring_entries) {
    // Read BAR0 (assume 32-bit MMIO BAR).
    uint32_t bar0 = pci_read_cfg(bus, dev, func, 0x10);
    uintptr_t mmio_base = (bar0 & ~0xFu); // mask type bits

    volatile uint32_t *mmio = (volatile uint32_t *)mmio_base;
    // Example device registers (device-specific offsets).
    const uint32_t REG_RING_BASE_LO = 0x100;
    const uint32_t REG_RING_BASE_HI = 0x104;
    const uint32_t REG_RING_LEN      = 0x108;
    const uint32_t REG_RING_HEAD     = 0x10C;
    const uint32_t REG_RING_TAIL     = 0x110;

    // Ensure ring memory is globally visible (cache flush if required).
    // Write ring base physical address to device.
    uint64_t phys_ring = (uint64_t)ring; // in identity-mapped environment
    mmio[REG_RING_BASE_LO/4] = (uint32_t)phys_ring;
    mmio[REG_RING_BASE_HI/4] = (uint32_t)(phys_ring >> 32);
    mmio[REG_RING_LEN/4] = ring_entries;
    mmio[REG_RING_HEAD/4] = 0;
    mmio[REG_RING_TAIL/4] = 0; // will be advanced as we enqueue
}