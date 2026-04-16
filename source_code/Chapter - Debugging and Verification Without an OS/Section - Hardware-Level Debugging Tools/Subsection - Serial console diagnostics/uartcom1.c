#include 

/* Low-level port I/O (x86_64, ring 0) */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* COM1 base port */
enum { COM1 = 0x3F8 };

/* Initialize 16550: 8N1, enable FIFO, set baud via divisor */
void uart_init(uint32_t uart_clock, uint32_t baud) {
    uint16_t base = COM1;
    uint16_t divisor = (uint16_t)(uart_clock / (16U * baud));

    outb(base + 1, 0x00);            /* Disable interrupts */
    outb(base + 3, 0x80);            /* Set DLAB = 1 to access divisor */
    outb(base + 0, (uint8_t)(divisor & 0xFF)); /* DLL */
    outb(base + 1, (uint8_t)(divisor >> 8));   /* DLM */
    outb(base + 3, 0x03);            /* 8 bits, no parity, one stop bit (DLAB=0) */
    outb(base + 2, 0x07);            /* Enable FIFO, clear RX/TX */
    outb(base + 4, 0x03);            /* Set RTS/DSR in MCR */
}

/* Wait for THR empty then write byte */
void uart_putc(char c) {
    uint16_t base = COM1;
    while ((inb(base + 5) & 0x20) == 0) { /* LSR bit 5: THR empty */
        asm volatile ("pause");           /* friendly spin */
    }
    outb(base + 0, (uint8_t)c);
}

/* Write NUL-terminated string */
void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r'); /* CRLF for many terminals */
        uart_putc(*s++);
    }
}

/* Print 64-bit value in hex (fast, no stdlib). */
void uart_puthex64(uint64_t v) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 15; i >= 0; --i) {
        uint8_t nib = (v >> (i*4)) & 0xF;
        uart_putc(hex[nib]);
    }
}

/* Example usage in early boot code
   uart_init(1843200, 115200);
   uart_puts("PANIC: register state:\n");
   uart_puthex64(cpu_rax); uart_putc('\n');
*/