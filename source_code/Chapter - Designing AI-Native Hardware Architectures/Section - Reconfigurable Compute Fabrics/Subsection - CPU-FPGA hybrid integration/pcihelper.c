#include 
/* I/O port access for x86; compile with GCC for bare-metal x64. */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t v;
    __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
/* Construct PCI config address for bus/slot/func/offset */
static uint32_t pci_cfg_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    return (uint32_t)((1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11)
           | ((uint32_t)func << 8) | (off & 0xFC));
}
static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    outl(0xCF8, pci_cfg_addr(bus, slot, func, off));
    return inl(0xCFC);
}
static void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val) {
    outl(0xCF8, pci_cfg_addr(bus, slot, func, off));
    outl(0xCFC, val);
}
/* Search bus 0 for vendor/device */
int pci_find_device(uint16_t target_vid, uint16_t target_did,
                    uint8_t *out_bus, uint8_t *out_slot, uint8_t *out_func) {
    for (uint8_t slot = 0; slot < 32; ++slot) {
        uint32_t id = pci_read32(0, slot, 0, 0x00);
        if (id == 0xFFFFFFFF) continue;
        uint16_t vid = id & 0xFFFF;
        uint16_t did = (id >> 16) & 0xFFFF;
        if (vid == target_vid && did == target_did) {
            *out_bus = 0; *out_slot = slot; *out_func = 0;
            return 0;
        }
    }
    return -1;
}
/* Simple physical DMA buffer, 4KiB aligned and page-sized. */
static volatile uint8_t dma_buf[4096] __attribute__((aligned(4096)));
/* Minimal workflow: enable MMIO and bus mastering, read BAR0, write descriptor. */
int setup_fpga(uint16_t vid, uint16_t did) {
    uint8_t bus, slot, func;
    if (pci_find_device(vid, did, &bus, &slot, &func)) return -1;
    uint32_t cmd = pci_read32(bus, slot, func, 0x04);
    cmd |= (1 << 2) | (1 << 1); /* bus master + memory space enable */
    pci_write32(bus, slot, func, 0x04, cmd);
    uint32_t bar0 = pci_read32(bus, slot, func, 0x10);
    /* If BAR0 indicates IO space, abort; we expect MMIO. */
    if (bar0 & 0x1) return -2;
    uint64_t mmio_base = (uint64_t)(bar0 & ~0xFULL);
    volatile uint32_t *mmio = (volatile uint32_t *)(uintptr_t)mmio_base;
    /* Program DMA descriptor: device-specific layout assumed */
    uintptr_t phys = (uintptr_t)dma_buf; /* bare-metal: physical==virtual */
    mmio[0] = (uint32_t)(phys & 0xFFFFFFFF);      /* DESC_ADDR_LO */
    mmio[1] = (uint32_t)((phys >> 32) & 0xFFFFFFFF); /* DESC_ADDR_HI */
    mmio[2] = 4096;                               /* DESC_LEN */
    mmio[3] = 1;                                  /* START */
    return 0;
}