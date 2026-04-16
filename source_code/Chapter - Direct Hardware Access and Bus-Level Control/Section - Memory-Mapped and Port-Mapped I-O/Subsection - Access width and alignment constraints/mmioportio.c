#include 
#include 

/* Compiler fence to prevent reordering of MMIO ops. */
static inline void compiler_fence(void) { asm volatile ("" ::: "memory"); }

/* MMIO read/write for 32/64-bit with alignment checks and read-back flush. */
static inline uint32_t mmio_read32(volatile void *addr) {
    assert(((uintptr_t)addr & 3) == 0);                       // require 4-byte alignment
    uint32_t val = *(volatile uint32_t *)addr;                // device read
    compiler_fence();
    return val;
}

static inline void mmio_write32(volatile void *addr, uint32_t val) {
    assert(((uintptr_t)addr & 3) == 0);
    *(volatile uint32_t *)addr = val;                         // device write (may be posted)
    compiler_fence();
    (void)mmio_read32(addr);                                  // read-back to ensure completion
}

static inline uint64_t mmio_read64(volatile void *addr) {
    assert(((uintptr_t)addr & 7) == 0);                       // require 8-byte alignment
    uint64_t val = *(volatile uint64_t *)addr;
    compiler_fence();
    return val;
}

static inline void mmio_write64(volatile void *addr, uint64_t val) {
    assert(((uintptr_t)addr & 7) == 0);
    *(volatile uint64_t *)addr = val;
    compiler_fence();
    (void)mmio_read64(addr);
}

/* Port I/O primitives: 8/16/32-bit only. Port in DX, size via register. */
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    asm volatile ("inb %w1, %0" : "=a"(v) : "d"(port));
    return v;
}
static inline uint16_t inw(uint16_t port) {
    uint16_t v;
    asm volatile ("inw %w1, %0" : "=a"(v) : "d"(port));
    return v;
}
static inline uint32_t inl(uint16_t port) {
    uint32_t v;
    asm volatile ("inl %w1, %0" : "=a"(v) : "d"(port));
    return v;
}
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %w1" : : "a"(val), "d"(port));
}
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %w1" : : "a"(val), "d"(port));
}
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %w1" : : "a"(val), "d"(port));
}