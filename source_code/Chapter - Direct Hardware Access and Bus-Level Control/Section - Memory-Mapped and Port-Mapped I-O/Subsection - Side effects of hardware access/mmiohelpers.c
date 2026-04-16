#include <stdint.h>

/* Write 32-bit MMIO register with ordering and readback to ensure completion. */
static inline void mmio_write32(volatile void *addr, uint32_t val) {
    __asm__ __volatile__("" ::: "memory");            // compiler barrier
    *(volatile uint32_t *)addr = val;                 // perform write
    __asm__ __volatile__("sfence" ::: "memory");     // order store visibility (x86)
    (void)*(volatile uint32_t *)addr;                 // readback forces PCIe completion
}

/* Read 32-bit MMIO register with a load fence to prevent reordering. */
static inline uint32_t mmio_read32(volatile void *addr) {
    uint32_t v = *(volatile uint32_t *)addr;          // destructive if device clears on read
    __asm__ __volatile__("lfence" ::: "memory");     // ensure read completes before later ops
    return v;
}

/* Port I/O helpers: inb/outb example (x86 I/O space). */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %b0, %w1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__ ("inb %w1, %b0" : "=a"(val) : "Nd"(port));
    return val;
}