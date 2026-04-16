#include 

/* Read 8-bit port */
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Write 8-bit port */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read 16-bit port */
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    asm volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Write 16-bit port */
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* Read 32-bit port */
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    asm volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Write 32-bit port */
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* Set PIT frequency on channel 0; frequency in Hz; executes in ring 0. */
static inline void pit_set_freq(uint32_t freq_hz) {
    const uint32_t PIT_BASE = 1193182U;
    uint32_t div = PIT_BASE / freq_hz;
    if (div == 0) div = 1;
    if (div > 0xFFFF) div = 0xFFFF;
    outb(0x43, 0x36);               /* control: channel0, lobyte/hibyte, mode 3 */
    outb(0x40, (uint8_t)(div & 0xFF));      /* low byte */
    outb(0x40, (uint8_t)((div >> 8) & 0xFF)); /* high byte */
}