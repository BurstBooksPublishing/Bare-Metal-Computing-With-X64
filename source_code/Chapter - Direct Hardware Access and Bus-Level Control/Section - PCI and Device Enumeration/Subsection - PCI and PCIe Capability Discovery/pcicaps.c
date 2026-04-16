#include <stdint.h>
#include <stdbool.h>

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Legacy config dword read via 0xCF8/0xCFC */
uint32_t pci_cfg_rd_dword_io(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) | ((uint32_t)fn << 8) | (off & 0xFC);
    outl(0xCF8, addr);
    return inl(0xCFC);
}

/* Enumerate classic PCI capabilities via IO method. Callback called with id and offset. */
int enumerate_pci_capabilities_io(uint8_t bus, uint8_t dev, uint8_t fn,
                                  void (*cb)(uint8_t id, uint8_t offset)) {
    uint32_t d = pci_cfg_rd_dword_io(bus, dev, fn, 0x04);
    uint16_t status = (uint16_t)(d >> 16);
    if (!(status & (1u << 4))) return 0; // no capability list

    uint8_t ptr = (uint8_t)(pci_cfg_rd_dword_io(bus, dev, fn, 0x34) & 0xFF);
    const int MAX = 48; // per Eq. (1)
    int count = 0;
    while (ptr && ptr >= 0x40 && ptr <= 0xFF && (ptr % 4 == 0) && count < MAX) {
        uint32_t entry = pci_cfg_rd_dword_io(bus, dev, fn, ptr & 0xFC);
        uint8_t capid = (uint8_t)(entry & 0xFF);
        cb(capid, ptr);
        uint8_t next = (uint8_t)((entry >> 8) & 0xFF);
        if (next == ptr) break; // self-loop guard
        ptr = next;
        ++count;
    }
    return count;
}

/* Enumerate PCIe extended caps using mapped MMCONFIG region.
   mmio_base points to the 4KB config block for device function. */
int enumerate_pcie_ext_caps_mmio(volatile uint32_t *mmio_base,
                                 void (*cb)(uint16_t id, uint8_t ver, uint32_t offset)) {
    if (!mmio_base) return 0;
    uint32_t off = 0x100;
    const int MAX_EXT = 960; // per Eq. (2)
    int count = 0;
    while (off >= 0x100 && off < 0x1000 && count < MAX_EXT) {
        uint32_t hdr = mmio_base[off/4];
        if ((hdr & 0xFFFF) == 0) break; // no capability
        uint16_t capid = (uint16_t)(hdr & 0xFFFF);
        uint8_t ver = (uint8_t)((hdr >> 16) & 0xF);
        uint32_t next = (hdr >> 20) & 0xFFF;
        cb(capid, ver, off);
        if (next == 0 || next == off) break;
        off = next;
        ++count;
    }
    return count;
}