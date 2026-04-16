#include 
#include 

/* Write a byte to a 16550 UART MMIO base (platform-specific). */
static inline void serial_write(uintptr_t uart_base, uint8_t b) {
    volatile uint8_t *port = (volatile uint8_t *)(uart_base + 0x0);
    *port = b; /* simple, busy write; platform must ensure MMIO space is mapped */
}

/* Atomically flip a bit in a 64-bit PTE and invalidate the corresponding VA.
   addr_va: virtual address whose PTE contains the bit to flip (for INVLPG)
   pte_ptr: pointer to the 64-bit PTE in physical-mapped memory
   bit: bit index [0..63] to toggle
   uart_base: MMIO base for serial logging */
void fault_inject_flip_bit(uintptr_t addr_va, volatile uint64_t *pte_ptr,
                           unsigned bit, uintptr_t uart_base) {
    uint64_t mask = (1ULL << (bit & 63));
    /* Disable interrupts deterministically */
    __asm__ volatile("cli" ::: "memory");

    /* Atomically toggle the bit using GCC builtin (maps to atomic XOR). */
    uint64_t before = __atomic_fetch_xor(pte_ptr, mask, __ATOMIC_SEQ_CST);

    /* Invalidate TLB entry for the virtual address touched. */
    __asm__ volatile("invlpg (%0)" :: "r"(addr_va) : "memory");

    /* Log the injection to serial (hex) for traceability. */
    uint64_t after = before ^ mask;
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (after >> i) & 0xF;
        uint8_t ch = nibble < 10 ? ('0' + nibble) : ('A' + (nibble - 10));
        serial_write(uart_base, ch);
    }
    serial_write(uart_base, '\n');

    /* Re-enable interrupts */
    __asm__ volatile("sti" ::: "memory");
}