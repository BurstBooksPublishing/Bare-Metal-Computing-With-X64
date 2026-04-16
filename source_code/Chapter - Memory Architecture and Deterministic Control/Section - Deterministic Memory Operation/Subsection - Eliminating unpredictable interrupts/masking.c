#include 
#define IOAPIC_REGSEL ((volatile uint32_t *)0xFEC00000)   // example base: platform-specific
#define IOAPIC_WINDOW ((volatile uint32_t *)(0xFEC00000 + 0x10))
#define APIC_BASE      ((volatile uint32_t *)0xFEE00000)  // local APIC base

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline void cli(void) { __asm__ volatile ("cli" ::: "memory"); }
static inline void sti(void) { __asm__ volatile ("sti" ::: "memory"); }

static void pic_mask_all(void) {
    outb(0x21, 0xFF); // master PIC mask
    outb(0xA1, 0xFF); // slave PIC mask
}

static void ioapic_write_redir(volatile uint32_t *ioapic, unsigned irq, uint64_t entry) {
    // IOREGSEL offset 0x00, IOWIN offset 0x10
    uint32_t idx_lo = 0x10 + irq*2;       // redirection low dword
    uint32_t idx_hi = idx_lo + 1;         // redirection high dword
    ioapic[0] = idx_lo;
    ioapic[4] = (uint32_t)entry;
    ioapic[0] = idx_hi;
    ioapic[4] = (uint32_t)(entry >> 32);
    // ensure write ordering
    __asm__ volatile ("mfence" ::: "memory");
}

static void lapic_write(uint32_t offset, uint32_t value) {
    volatile uint32_t *reg = (volatile uint32_t *)((uintptr_t)APIC_BASE + offset);
    *reg = value;
    __asm__ volatile ("mfence" ::: "memory");
}

void enter_interrupt_free_region(void) {
    cli();                      // stop maskable INTR delivery to IF
    pic_mask_all();             // mask legacy PIC interrupts
    // mask IOAPIC entries for IRQs 0..23 (platform dependent)
    for (unsigned irq = 0; irq < 24; ++irq) {
        uint64_t redir = (1ULL<<63); // set mask bit in redirection entry
        ioapic_write_redir(IOAPIC_REGSEL, irq, redir);
    }
    // set TPR to 0xFF to suppress APIC delivery of all vectors < 0xFF
    lapic_write(0x80, 0xFF000000u >> 24); // TPR at offset 0x80; write as 32-bit
    // mask local APIC LVT timer and LINTs (offsets platform dependent)
    lapic_write(0x320, 1u<<16); // LVT Timer: mask bit
    lapic_write(0x350, 1u<<16); // LVT LINT0: mask
    lapic_write(0x360, 1u<<16); // LVT LINT1: mask
    // Caller must ensure IOMMU and PCI MSI are disabled separately.
}