#include 
#include 

/* MMIO base provided by platform; identity mapping expected in bare-metal. */
#define WDT_BASE 0xFEC00000UL

/* Register offsets (platform-specific) */
#define WDT_CTRL_OFF   0x00  /* control: enable, timeout enable */
#define WDT_RELOAD_OFF 0x04  /* reload value in milliseconds */
#define WDT_SEQ_OFF    0x08  /* pet sequence register (unlock+write) */
#define WDT_STATUS_OFF 0x0C  /* readonly status */

/* Secure unlock constant required by controller to accept a pet. */
#define WDT_UNLOCK_KEY 0x5A5A5A5AUL

static inline void mmio_write32(uintptr_t addr, uint32_t value) {
    volatile uint32_t *p = (volatile uint32_t *)addr;
    *p = value;
    asm volatile ("" ::: "memory"); /* compiler barrier */
}

static inline uint32_t mmio_read32(uintptr_t addr) {
    volatile uint32_t *p = (volatile uint32_t *)addr;
    uint32_t v = *p;
    asm volatile ("" ::: "memory");
    return v;
}

/* Initialize watchdog with timeout_ms and enable. Returns 0 on success. */
int wdt_init(unsigned timeout_ms) {
    if (timeout_ms == 0) return -1;
    mmio_write32(WDT_BASE + WDT_RELOAD_OFF, (uint32_t)timeout_ms);
    /* Set enable bit (bit0). Other bits are platform-defined. */
    mmio_write32(WDT_BASE + WDT_CTRL_OFF, 0x1);
    return 0;
}

/* Securely "pet" the watchdog. Must be called before timeout. */
void wdt_pet(void) {
    /* Write unlock key followed by reload value atomically if required. */
    mmio_write32(WDT_BASE + WDT_SEQ_OFF, WDT_UNLOCK_KEY);
    /* A small delay or barrier may be required by specific HW */
    asm volatile ("nop");
    mmio_write32(WDT_BASE + WDT_SEQ_OFF, 0x1); /* final pet command */
}

/* Query status: 0==ok, nonzero==fault */
int wdt_status(void) {
    return (int)mmio_read32(WDT_BASE + WDT_STATUS_OFF);
}