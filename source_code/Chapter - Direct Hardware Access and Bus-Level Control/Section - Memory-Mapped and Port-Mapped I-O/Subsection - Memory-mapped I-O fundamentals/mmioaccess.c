#include 

/* Write 32-bit MMIO register with ordering and write-completion guarantee. */
static inline void mmio_write32(volatile void *base, uint32_t offset, uint32_t value)
{
    volatile uint32_t *ptr = (volatile uint32_t *)((uintptr_t)base + offset);
    *ptr = value;                      /* compiler emits a 32-bit store to MMIO */
    __asm__ __volatile__("sfence" ::: "memory"); /* order prior stores if WC */
    /* Readback to force completion of posted writes on PCIe */
    (void)*ptr;
    __asm__ __volatile__("" ::: "memory"); /* compiler barrier */
}

/* Read 32-bit MMIO register with compiler/CPU ordering guarantees. */
static inline uint32_t mmio_read32(volatile void *base, uint32_t offset)
{
    volatile uint32_t *ptr = (volatile uint32_t *)((uintptr_t)base + offset);
    uint32_t val = *ptr;               /* volatile load from device */
    __asm__ __volatile__("lfence" ::: "memory"); /* optional: serialize subsequent loads */
    return val;
}

/* Example: write command 0x1 to control register at offset 0x10 and wait for status. */
static inline int device_command_and_wait(volatile void *base)
{
    mmio_write32(base, 0x10, 0x1);                /* start command */
    for (int i = 0; i < 1000000; ++i) {
        uint32_t st = mmio_read32(base, 0x14);    /* poll status register */
        if (st & 0x1) return 0;                  /* success */
    }
    return -1; /* timeout */
}