#include 

// Write a byte to an I/O port (x86_64, ring 0). Compiler barrier ensures order.
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Set PIT channel 0 to 'hz' frequency. hz must be >0.
void pit_set_frequency(uint32_t hz) {
    const uint32_t PIT_CLOCK = 1193182U;   // input clock in Hz
    uint32_t divisor;

    if (hz == 0) return;                   // avoid div/0
    divisor = (PIT_CLOCK + hz/2) / hz;     // rounded integer division
    if (divisor < 1) divisor = 1;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    uint8_t lo = (uint8_t)(divisor & 0xFF);
    uint8_t hi = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x43, 0x36);   // control: channel 0, lobyte/hibyte, mode 3, binary
    outb(0x40, lo);     // write low byte
    outb(0x40, hi);     // write high byte
}

// Example usage in boot code:
// pit_set_frequency(1000); // request 1 kHz periodic PIT interrupts