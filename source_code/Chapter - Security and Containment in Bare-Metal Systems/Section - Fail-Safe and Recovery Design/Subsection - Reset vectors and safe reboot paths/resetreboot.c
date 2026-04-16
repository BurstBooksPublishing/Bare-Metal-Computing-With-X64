#include 
/* low-level I/O and MMIO primitives (x86_64 GCC/Clang) */

// write a byte to an IO port
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

// write 32-bit to memory-mapped IO (volatile)
static inline void write_mmio(uintptr_t addr, uint32_t val) {
    volatile uint32_t *p = (volatile uint32_t *)addr;
    *p = val;
    asm volatile ("" ::: "memory"); // compiler barrier
}

// cause triple fault by loading null IDT and forcing an interrupt
static void force_triple_fault(void) {
    struct { uint16_t limit; uintptr_t base; } __attribute__((packed)) idtr = {0, 0};
    asm volatile ("lidt %0" :: "m"(idtr) : "memory");
    asm volatile ("int $3"); // triggers exception that will escalate to triple fault
    for(;;) asm volatile ("hlt");
}

// generic reset: try ACPI (if GAS provided), then CF9, KBC, then triple fault
void safe_reboot(bool have_acpi, uint8_t acpi_space_id, uintptr_t acpi_addr, uint8_t reset_value) {
    asm volatile ("cli"); // disable interrupts to make reboot deterministic

    if (have_acpi) {
        if (acpi_space_id == 0) { // system IO
            outb((uint16_t)acpi_addr, reset_value);
        } else if (acpi_space_id == 1) { // system memory
            write_mmio(acpi_addr, (uint32_t)reset_value);
        }
        // give firmware a moment
        for (volatile int i = 0; i < 1000000; ++i) asm volatile ("pause");
    }

    // chipset reset via 0xCF9: bit 1 (0x02) = full reset request when combined with bit0 (0x01)
    outb(0xCF9, 0x06); // recommended on modern PCs
    for (volatile int i = 0; i < 100000; ++i) asm volatile ("pause");

    // legacy keyboard controller reset (best-effort)
    outb(0x64, 0xFE);
    for (volatile int i = 0; i < 100000; ++i) asm volatile ("pause");

    // final fallback
    force_triple_fault();
}