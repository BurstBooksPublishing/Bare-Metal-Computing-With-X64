#include 

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* PCI config space via CONFIG_ADDR/CONFIG_DATA (legacy) */
uint32_t pci_cfg_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    uint32_t addr = 0x80000000u
        | ((uint32_t)bus << 16)
        | ((uint32_t)dev << 11)
        | ((uint32_t)fn  << 8)
        | (off & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}
void pci_cfg_write32(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t v) {
    uint32_t addr = 0x80000000u
        | ((uint32_t)bus << 16)
        | ((uint32_t)dev << 11)
        | ((uint32_t)fn  << 8)
        | (off & 0xFC);
    outl(0xCF8, addr);
    outl(0xCFC, v);
}

/* Disable Bus Mastering on a device to stop DMA until policy applied */
void pci_disable_bus_master(uint8_t bus, uint8_t dev, uint8_t fn) {
    uint32_t cmd = pci_cfg_read32(bus, dev, fn, 0x04);
    cmd &= ~(1u << 2); /* clear Bus Master Enable */
    pci_cfg_write32(bus, dev, fn, 0x04, cmd);
}

/* Map physical [phys, phys+size) into existing 4-level page tables at virt
   with flags (present, rw, nx encoded by absence of NX flag). Caller ensures
   page tables allocated and CR3 is known. This example assumes 4K pages. */
void map_physical_region(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags) {
    const uint64_t P = 4096;
    uint64_t pages = (size + P - 1) / P;
    for (uint64_t i = 0; i < pages; ++i) {
        uint64_t va = virt + i * P;
        uint64_t pa = phys + i * P;
        /* compute indices and walk/create PTs; omitted: allocation helpers */
        /* set PTE = (pa & ~0xFFF) | flags | 1 (present) */
        /* platform-specific: flush TLB entries for va after mapping */
    }
}