#include 
#include 

#define PCI_CFG_COMMAND 0x04U
#define PCI_CMD_BUS_MASTER (1U << 2)

// ECAM base known from platform (procured from ACPI DMAR table or firmware).
// Replace with actual ECAM base for the platform.
static volatile uint8_t *const ECAM_BASE = (volatile uint8_t *)0xE0000000ULL;

// Read 32-bit config at bus/dev/func/reg_offset
static inline uint32_t pci_cfg_read32(unsigned bus, unsigned dev, unsigned func, unsigned offset) {
    uint64_t addr = (uint64_t)ECAM_BASE
                  + ((uint64_t)bus << 20)
                  + ((uint64_t)dev << 15)
                  + ((uint64_t)func << 12)
                  + (offset & 0xFFF);
    volatile uint32_t *ptr = (volatile uint32_t *)addr;
    return *ptr; // MMIO read
}

// Write 32-bit config
static inline void pci_cfg_write32(unsigned bus, unsigned dev, unsigned func, unsigned offset, uint32_t val) {
    uint64_t addr = (uint64_t)ECAM_BASE
                  + ((uint64_t)bus << 20)
                  + ((uint64_t)dev << 15)
                  + ((uint64_t)func << 12)
                  + (offset & 0xFFF);
    volatile uint32_t *ptr = (volatile uint32_t *)addr;
    *ptr = val; // MMIO write
}

// Disable bus-mastering on a function; returns previous command register.
uint32_t pci_disable_bus_master(unsigned bus, unsigned dev, unsigned func) {
    uint32_t cmd = pci_cfg_read32(bus, dev, func, PCI_CFG_COMMAND);
    uint32_t prev = cmd;
    if (cmd & PCI_CMD_BUS_MASTER) {
        cmd &= ~PCI_CMD_BUS_MASTER;
        pci_cfg_write32(bus, dev, func, PCI_CFG_COMMAND, cmd);
    }
    return prev;
}

/* Example usage:
   uint32_t old = pci_disable_bus_master(0, 25, 0); // bus 0, device 25, function 0
*/