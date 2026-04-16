#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PCI_CONF_VENDOR_ID 0x00
#define PCI_CONF_DEVICE_ID 0x02
#define PCI_CONF_COMMAND   0x04
#define PCI_CONF_HEADER    0x0E
#define PCI_CONF_CLASS     0x0B
#define PCI_CONF_BAR0      0x10
#define PCI_CONF_STATUS    0x06

static volatile uint8_t *ecam_base;     // set to mapped ECAM base
static uintptr_t mmio_alloc_base = 0x80000000ULL; // runtime MMIO allocator

static inline uint32_t ecam_read32(unsigned bus, unsigned dev, unsigned fn, uint32_t off) {
    uintptr_t addr = (uintptr_t)ecam_base + (bus << 20) + (dev << 15) + (fn << 12) + off;
    return *(volatile uint32_t *)addr;
}

static inline void ecam_write32(unsigned bus, unsigned dev, unsigned fn, uint32_t off, uint32_t val) {
    uintptr_t addr = (uintptr_t)ecam_base + (bus << 20) + (dev << 15) + (fn << 12) + off;
    *(volatile uint32_t *)addr = val;
}

static uint64_t probe_bar_size(unsigned bus, unsigned dev, unsigned fn, uint32_t bar_off) {
    uint32_t orig = ecam_read32(bus, dev, fn, bar_off);
    if ((orig & 0x1) == 0x1) { // IO space
        ecam_write32(bus, dev, fn, bar_off, 0xFFFFFFFF);
        uint32_t r = ecam_read32(bus, dev, fn, bar_off) & 0xFFFFFFFC;
        ecam_write32(bus, dev, fn, bar_off, orig);
        return (uint64_t)((~r) + 1);
    } else { // Memory space (could be 64-bit)
        uint32_t type = orig & 0x6;
        if (type == 0x4) { // 64-bit BAR
            uint32_t orig_hi = ecam_read32(bus, dev, fn, bar_off + 4);
            ecam_write32(bus, dev, fn, bar_off, 0xFFFFFFFF);
            ecam_write32(bus, dev, fn, bar_off + 4, 0xFFFFFFFF);
            uint32_t r_lo = ecam_read32(bus, dev, fn, bar_off) & 0xFFFFFFF0;
            uint32_t r_hi = ecam_read32(bus, dev, fn, bar_off + 4);
            ecam_write32(bus, dev, fn, bar_off, orig);
            ecam_write32(bus, dev, fn, bar_off + 4, orig_hi);
            uint64_t mask = ((uint64_t)r_hi << 32) | r_lo;
            return (~mask) + 1;
        } else { // 32-bit BAR
            ecam_write32(bus, dev, fn, bar_off, 0xFFFFFFFF);
            uint32_t r = ecam_read32(bus, dev, fn, bar_off) & 0xFFFFFFF0;
            ecam_write32(bus, dev, fn, bar_off, orig);
            return (uint64_t)((~r) + 1);
        }
    }
}

static void assign_bar(unsigned bus, unsigned dev, unsigned fn, uint32_t bar_off, uint64_t size) {
    // align mmio_alloc_base
    uintptr_t base = (mmio_alloc_base + (size - 1)) & ~(size - 1);
    if ((ecam_read32(bus, dev, fn, bar_off) & 0x1) == 0x1) {
        // IO BAR assignment would use legacy IO allocator (not shown)
    } else {
        // write lower dword
        ecam_write32(bus, dev, fn, bar_off, (uint32_t)base);
        // if 64-bit write high dword
        uint32_t typ = ecam_read32(bus, dev, fn, bar_off) & 0x6;
        if (typ == 0x4) ecam_write32(bus, dev, fn, bar_off + 4, (uint32_t)(base >> 32));
        mmio_alloc_base = base + size;
    }
}

static void enable_device(unsigned bus, unsigned dev, unsigned fn, bool io_required) {
    uint32_t cmd = ecam_read32(bus, dev, fn, PCI_CONF_COMMAND);
    cmd |= (1 << 1) | (1 << 2); // Memory space and Bus Master
    if (io_required) cmd |= 1;  // IO space if needed
    ecam_write32(bus, dev, fn, PCI_CONF_COMMAND, cmd);
}

static void scan_function(unsigned bus, unsigned dev, unsigned fn) {
    uint32_t vid = ecam_read32(bus, dev, fn, PCI_CONF_VENDOR_ID) & 0xFFFF;
    if (vid == 0xFFFF) return;
    uint8_t header = (ecam_read32(bus, dev, fn, PCI_CONF_HEADER) >> 16) & 0xFF;
    // probe BARs (max 6 for header type 0)
    for (uint32_t off = PCI_CONF_BAR0; off < PCI_CONF_BAR0 + 6*4; off += 4) {
        uint32_t bar = ecam_read32(bus, dev, fn, off);
        if (bar == 0) continue;
        uint64_t size = probe_bar_size(bus, dev, fn, off);
        if (size) assign_bar(bus, dev, fn, off, size);
    }
    // enable device (simplified: assume memory BARs used)
    enable_device(bus, dev, fn, false);
    // if header == 1, handle bridge registers (not fully implemented here)
}

static void enumerate_bus(unsigned bus) {
    for (unsigned dev = 0; dev < 32; ++dev) {
        uint32_t vid = ecam_read32(bus, dev, 0, PCI_CONF_VENDOR_ID) & 0xFFFF;
        if (vid == 0xFFFF) continue;
        uint8_t hdr = (ecam_read32(bus, dev, 0, PCI_CONF_HEADER) >> 16) & 0xFF;
        unsigned max_fn = (hdr & 0x80) ? 8 : 1;
        for (unsigned fn = 0; fn < max_fn; ++fn)
            scan_function(bus, dev, fn);
    }
}