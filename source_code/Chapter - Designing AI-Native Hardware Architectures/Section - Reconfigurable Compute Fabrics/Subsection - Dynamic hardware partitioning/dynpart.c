#include 
#include 

#define CFG_CTRL_OFF 0x00   /* write: command register (vendor-specific) */
#define CFG_STATUS_OFF 0x04 /* read: status bits */
#define CFG_ERR_OFF 0x08    /* read: error code */
#define CFG_ARG_OFF 0x0C    /* write: argument (e.g., partition id) */

#define CTRL_CMD_RECONFIG 1u
#define STATUS_DONE_MASK  (1u<<0)
#define STATUS_ERR_MASK   (1u<<1)

#define TIMEOUT_CYCLES 10000000u

static inline void mmio_write32(volatile void *base, size_t off, uint32_t v)
{
    *((volatile uint32_t *)((uintptr_t)base + off)) = v;
    __asm__ volatile("sfence" ::: "memory"); /* ensure ordering */
}

static inline uint32_t mmio_read32(volatile void *base, size_t off)
{
    uint32_t v = *((volatile uint32_t *)((uintptr_t)base + off));
    __asm__ volatile("" ::: "memory");
    return v;
}

/* Reconfigure partition 'part_id'. base is MMIO BAR virtual base. */
int reconfigure_partition(volatile void *base, uint32_t part_id)
{
    uint32_t timeout = TIMEOUT_CYCLES;
    /* 1) Command argument: which partition or bitstream slot. */
    mmio_write32(base, CFG_ARG_OFF, part_id);
    /* 2) Issue reconfiguration command. */
    mmio_write32(base, CFG_CTRL_OFF, CTRL_CMD_RECONFIG);
    /* 3) Poll for completion with timeout. */
    while (timeout--) {
        uint32_t st = mmio_read32(base, CFG_STATUS_OFF);
        if (st & STATUS_DONE_MASK) {
            if (st & STATUS_ERR_MASK) return -2; /* device signalled error */
            return 0; /* success */
        }
    }
    return -1; /* timeout */
}