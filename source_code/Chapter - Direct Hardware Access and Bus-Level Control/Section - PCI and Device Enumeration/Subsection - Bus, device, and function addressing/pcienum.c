#include 

// Port IO wrappers for x86/x86_64 bare metal.
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "d"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    asm volatile ("inl %1, %0" : "=a"(val) : "d"(port));
    return val;
}

// Read a dword from PCI config space via Mechanism 1.
uint32_t pci_config_read32(uint8_t bus, uint8_t device,
                           uint8_t function, uint8_t offset) {
    uint32_t addr = (uint32_t)((1U << 31) |
                  ((uint32_t)bus << 16) |
                  ((uint32_t)device << 11) |
                  ((uint32_t)function << 8) |
                  ((uint32_t)offset & 0xFC));
    outl(0xCF8, addr);                // write CONFIG_ADDRESS
    return inl(0xCFC);                // read CONFIG_DATA
}

// Probe and print matching devices (hook to your console putchar/printf).
void enumerate_bus(uint8_t bus) {
    for (uint8_t dev = 0; dev < 32; ++dev) {
        uint32_t v = pci_config_read32(bus, dev, 0, 0x00);
        uint16_t vendor = (uint16_t)(v & 0xFFFF);
        if (vendor == 0xFFFF) continue;   // no device present
        uint16_t device_id = (uint16_t)(v >> 16);
        uint32_t hdr = pci_config_read32(bus, dev, 0, 0x0C);
        uint8_t header_type = (hdr >> 16) & 0xFF;
        int multifunction = header_type & 0x80;
        for (uint8_t fn = 0; fn < (multifunction ? 8 : 1); ++fn) {
            uint32_t vv = pci_config_read32(bus, dev, fn, 0x00);
            uint16_t ven = (uint16_t)(vv & 0xFFFF);
            if (ven == 0xFFFF) continue;
            // Handle device: read class/subclass (0x08), BARs (0x10..), etc.
            // Example: check if it's a bridge and recurse.
            uint32_t cls = pci_config_read32(bus, dev, fn, 0x08);
            uint8_t prog_if = (cls >> 8) & 0xFF;
            uint8_t subclass = (cls >> 16) & 0xFF;
            uint8_t class_code = (cls >> 24) & 0xFF;
            // If PCI-to-PCI bridge, get secondary bus number and recurse.
            if (class_code == 0x06 && subclass == 0x04) {
                uint32_t br = pci_config_read32(bus, dev, fn, 0x18);
                uint8_t sec_bus = (br >> 8) & 0xFF;
                enumerate_bus(sec_bus);
            }
            // TODO: record device for initialization.
        }
    }
}