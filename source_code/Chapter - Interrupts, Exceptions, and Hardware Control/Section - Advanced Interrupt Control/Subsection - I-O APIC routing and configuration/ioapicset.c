#include 
#include 
#include 

// MMIO base is the physical mapping of the IOAPIC's register window.
static inline void ioapic_write32(volatile void *base, uint8_t reg, uint32_t val)
{
    volatile uint32_t *mmio = (volatile uint32_t *)base;
    mmio[0x00/4] = reg;             // IOREGSEL
    __asm__ volatile ("" ::: "memory");
    mmio[0x10/4] = val;             // IOWIN
    __asm__ volatile ("" ::: "memory");
}

static inline uint32_t ioapic_read32(volatile void *base, uint8_t reg)
{
    volatile uint32_t *mmio = (volatile uint32_t *)base;
    mmio[0x00/4] = reg;
    __asm__ volatile ("" ::: "memory");
    uint32_t v = mmio[0x10/4];
    __asm__ volatile ("" ::: "memory");
    return v;
}

// Get number of redirection entries.
static inline unsigned ioapic_entries(volatile void *base)
{
    uint32_t ver = ioapic_read32(base, 0x01);
    return ((ver >> 16) & 0xFFu) + 1u;
}

// Configure a single redirection entry (IRQ number 'irq').
void ioapic_set_redir(volatile void *base, unsigned irq,
                      uint8_t vector, uint8_t delivery_mode,
                      bool dest_logical, uint8_t dest_apic_id,
                      bool polarity_low, bool level_trigger, bool mask)
{
    unsigned entries = ioapic_entries(base);
    if (irq >= entries) return; // out-of-range, caller must verify

    uint8_t idx_low = (uint8_t)(0x10 + 2 * irq);
    uint8_t idx_high = (uint8_t)(idx_low + 1);

    uint32_t low = (uint32_t)vector;
    low |= ((uint32_t)(delivery_mode & 0x7) << 8);
    low |= (dest_logical ? (1u << 11) : 0u);
    low |= (polarity_low ? (1u << 13) : 0u);
    low |= (level_trigger ? (1u << 15) : 0u);
    low |= (mask ? (1u << 16) : 0u);

    uint32_t high = ((uint32_t)dest_apic_id) << 24;

    // Write high then low on some hardware to avoid transient assertion.
    ioapic_write32(base, idx_high, high);
    ioapic_write32(base, idx_low, low);
}