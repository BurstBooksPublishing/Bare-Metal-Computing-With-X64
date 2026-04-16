#include 
#include 

#define PCI_CONFIG_ADDR_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT 0xCFC

/* Port I/O primitives (x86_64 inline asm). */
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" :: "a"(val), "d"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    asm volatile ("inl %1, %0" : "=a"(val) : "d"(port));
    return val;
}

/* Read/write 32-bit PCI config via mechanism 1. */
static inline uint32_t pci_cfg_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t addr = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)dev << 11)
                    | ((uint32_t)func << 8) | (off & 0xFC);
    outl(PCI_CONFIG_ADDR_PORT, addr);
    return inl(PCI_CONFIG_DATA_PORT + (off & 3));
}
static inline void pci_cfg_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t val) {
    uint32_t addr = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)dev << 11)
                    | ((uint32_t)func << 8) | (off & 0xFC);
    outl(PCI_CONFIG_ADDR_PORT, addr);
    outl(PCI_CONFIG_DATA_PORT + (off & 3), val);
}

typedef struct {
    uint64_t base;
    uint64_t size;
    uint32_t flags;       /* original low dword flags */
    bool is_io;
    bool is_64;
    bool prefetchable;
} pci_bar_info_t;

pci_bar_info_t probe_pci_bar(uint8_t bus, uint8_t dev, uint8_t func, unsigned bar_index) {
    pci_bar_info_t r = {0};
    uint8_t off = 0x10 + (bar_index * 4);
    uint32_t orig = pci_cfg_read32(bus, dev, func, off);

    /* Write all ones, read mask, then restore original. */
    pci_cfg_write32(bus, dev, func, off, 0xFFFFFFFF);
    uint32_t mask_lo = pci_cfg_read32(bus, dev, func, off);
    pci_cfg_write32(bus, dev, func, off, orig);

    r.is_io = (orig & 0x1) != 0;
    r.flags = orig & (r.is_io ? 0x3u : 0xFu);
    if (r.is_io) {
        uint32_t masked = mask_lo & 0xFFFFFFFCu;
        r.size = (uint64_t)((~masked) + 1u);
        r.base = (uint64_t)(orig & 0xFFFFFFFCu);
        r.is_64 = false;
        r.prefetchable = false;
    } else {
        uint32_t type = (orig >> 1) & 0x3;
        r.prefetchable = (orig >> 3) & 0x1;
        if (type == 0x2) {
            /* 64-bit BAR: probe high dword as well. */
            uint32_t orig_hi = pci_cfg_read32(bus, dev, func, off + 4);
            pci_cfg_write32(bus, dev, func, off + 4, 0xFFFFFFFF);
            uint32_t mask_hi = pci_cfg_read32(bus, dev, func, off + 4);
            pci_cfg_write32(bus, dev, func, off + 4, orig_hi);

            uint64_t mask64 = ((uint64_t)mask_hi << 32) | (mask_lo & 0xFFFFFFF0u);
            uint64_t base64 = ((uint64_t)orig_hi << 32) | (orig & 0xFFFFFFF0u);
            r.size = (~mask64) + 1ULL;
            r.base = base64;
            r.is_64 = true;
        } else {
            uint32_t masked = mask_lo & 0xFFFFFFF0u;
            r.size = (uint64_t)((~masked) + 1u);
            r.base = (uint64_t)(orig & 0xFFFFFFF0u);
            r.is_64 = false;
        }
    }
    return r;
}
/* Usage: call probe_pci_bar() for each BAR, then map returned base/size into page tables. */