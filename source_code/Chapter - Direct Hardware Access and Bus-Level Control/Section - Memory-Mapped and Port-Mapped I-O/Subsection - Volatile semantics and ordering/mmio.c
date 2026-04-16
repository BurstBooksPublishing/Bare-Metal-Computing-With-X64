#include <stdint.h>

/* Compiler barrier only */
static inline void compiler_barrier(void) {
    asm volatile ("" ::: "memory");
}

/* MMIO write: volatile store then MFENCE to ensure device sees it. */
static inline void mmio_write32(volatile void *addr, uint32_t val) {
    compiler_barrier();                                    /* prevent reordering with surrounding code */
    *(volatile uint32_t *)addr = val;                      /* uncached device store */
    asm volatile("mfence" ::: "memory");                   /* ensure visibility to device and other agents */
}

/* MMIO read: LFENCE or MFENCE can be used to serialize; MFENCE is conservative. */
static inline uint32_t mmio_read32(volatile void *addr) {
    uint32_t v;
    compiler_barrier();
    v = *(volatile uint32_t *)addr;                        /* read from device */
    asm volatile("lfence" ::: "memory");                   /* prevent following loads from moving before this read */
    return v;
}

/* Port I/O using inl/outl. Use MFENCE when mixing MMIO and port I/O ordering matters. */
static inline void io_out32(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %w1" : : "a"(val), "Nd"(port) : ); /* volatile asm is compiler barrier for this instr */
}

/* Read from port */
static inline uint32_t io_in32(uint16_t port) {
    uint32_t val;
    asm volatile ("inl %w1, %0" : "=a"(val) : "Nd"(port) : );
    return val;
}