#include 
#include 

#define MMIO_BASE   ((volatile uint32_t*)0xF0000000U) /* device BAR0 base */
#define REG_OFFSET(x) ((volatile uint32_t*)((uintptr_t)MMIO_BASE + (x)))

/* Offsets (example, vendor-specific) */
enum {
    OFF_CTRL     = 0x00, /* control: bit0=PR_START, bit1=START_ACCL */
    OFF_STATUS   = 0x04, /* status: bit0=PR_DONE, bit1=ACCL_BUSY */
    OFF_DOORBELL = 0x08, /* doorbell: start token / stream id */
    OFF_DMA_CMD  = 0x0C, /* DMA start: src_addr low */
    OFF_DMA_ARG  = 0x10  /* DMA arg: length or flags */
};

/* MMIO access helpers */
static inline void mmio_write(uintptr_t off, uint32_t v) {
    *REG_OFFSET(off) = v;
    __asm__ volatile ("" : : : "memory"); /* prevent reordering */
}
static inline uint32_t mmio_read(uintptr_t off) {
    uint32_t v = *REG_OFFSET(off);
    __asm__ volatile ("" : : : "memory");
    return v;
}

/* Trigger partial reconfiguration and wait for completion */
bool trigger_pr_and_wait(void) {
    mmio_write(OFF_CTRL, 0x1U); /* PR_START */
    for (volatile int i = 0; i < 1000000; ++i) {
        uint32_t s = mmio_read(OFF_STATUS);
        if (s & 0x1U) return true; /* PR_DONE */
    }
    return false; /* timeout */
}

/* Start accelerator after PR and DMA complete */
bool start_accelerator(uintptr_t weight_phys, uint32_t length) {
    mmio_write(OFF_DMA_CMD, (uint32_t)weight_phys); /* low 32 bits */
    mmio_write(OFF_DMA_ARG, length);
    /* kick DMA via control bit 2 (example) */
    mmio_write(OFF_CTRL, 0x4U);
    /* wait for DMA completion (poll ACCL_BUSY cleared) */
    for (volatile int i = 0; i < 1000000; ++i) {
        uint32_t s = mmio_read(OFF_STATUS);
        if ((s & 0x2U) == 0) break;
    }
    /* start accelerator via doorbell */
    mmio_write(OFF_DOORBELL, 1U);
    mmio_write(OFF_CTRL, 0x2U); /* START_ACCL */
    return true;
}