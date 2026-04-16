#include 

/* I/O port primitives for x86 bare-metal (use proper inline asm). */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Build PCI config address for mechanism 1. */
static inline uint32_t pci_cfg_addr(uint8_t bus, uint8_t dev,
                                    uint8_t func, uint8_t offset) {
    return 0x80000000U | ((uint32_t)bus << 16) | ((uint32_t)dev << 11)
           | ((uint32_t)func << 8) | (offset & 0xFC);
}

/* Read/write 32-bit config dword via 0xCF8/0xCFC. */
uint32_t pci_read_config_dword(uint8_t bus, uint8_t dev,
                               uint8_t func, uint8_t offset) {
    outl(0xCF8, pci_cfg_addr(bus, dev, func, offset));
    return inl(0xCFC);
}
void pci_write_config_dword(uint8_t bus, uint8_t dev,
                            uint8_t func, uint8_t offset, uint32_t val) {
    outl(0xCF8, pci_cfg_addr(bus, dev, func, offset));
    outl(0xCFC, val);
}

/* Enable Bus Mastering by setting Command register bit 2. */
void pci_enable_bus_master(uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t cmd = pci_read_config_dword(bus, dev, func, 0x04);
    cmd |= 0x4U; /* Bus Master Enable */
    pci_write_config_dword(bus, dev, func, 0x04, cmd);
}

/* Probe BAR0: return base_phys and size. */
int pci_probe_bar0(uint8_t bus, uint8_t dev, uint8_t func,
                   uint64_t *base_phys, uint64_t *size) {
    uint32_t orig = pci_read_config_dword(bus, dev, func, 0x10);
    /* If BAR indicates IO space (bit0==1), not MMIO. */
    if (orig & 0x1U) return -1;
    /* Write all ones to discover size. */
    pci_write_config_dword(bus, dev, func, 0x10, 0xFFFFFFFFU);
    uint32_t mask = pci_read_config_dword(bus, dev, func, 0x10);
    /* Restore original BAR value. */
    pci_write_config_dword(bus, dev, func, 0x10, orig);
    uint32_t psz_mask = mask & 0xFFFFFFF0U;
    if (psz_mask == 0) return -1;
    uint64_t sz = (~(uint64_t)psz_mask & 0xFFFFFFFFULL) + 1ULL;
    *base_phys = (uint64_t)(orig & 0xFFFFFFF0U);
    *size = sz;
    return 0;
}

/* High-level setup routine. */
int configure_device_for_dma(uint8_t bus, uint8_t dev, uint8_t func) {
    uint64_t base, len;
    pci_enable_bus_master(bus, dev, func);
    if (pci_probe_bar0(bus, dev, func, &base, &len) != 0) return -1;
    /* Map MMIO region into CPU address space (identity map or chosen VA). */
    extern void map_mmio_range(uint64_t phys, void* virt, uint64_t size);
    map_mmio_range(base, (void*)base, len); /* identity map for simplicity */
    return 0;
}