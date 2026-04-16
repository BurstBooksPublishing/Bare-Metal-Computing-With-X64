#include 
#include 

/* Offsets for a generic MMIO WDT device; replace per platform datasheet */
#define WDT_CTRL_OFF    0x00  /* control: bit0 = enable */
#define WDT_TIMEOUT_OFF 0x04  /* timeout value in ms */
#define WDT_KICK_OFF    0x08  /* write-any-value to pet */
#define WDT_LOCK_OFF    0x0C  /* write magic to lock config */

static inline void mmio_write32(volatile void *base, size_t off, uint32_t v) {
    *((volatile uint32_t *)((uintptr_t)base + off)) = v;
    asm volatile ("" ::: "memory"); /* compiler barrier */
}
static inline uint32_t mmio_read32(volatile void *base, size_t off) {
    asm volatile ("" ::: "memory");
    return *((volatile uint32_t *)((uintptr_t)base + off));
}

/* Initialize watchdog: base is mapped MMIO region; timeout_ms per platform limits */
int wdt_init(volatile void *base, uint32_t timeout_ms) {
    if (!base || timeout_ms == 0) return -1;
    /* Set timeout before enabling to avoid immediate reset at enable */
    mmio_write32(base, WDT_TIMEOUT_OFF, timeout_ms);
    /* Enable WDT: set bit0 */
    mmio_write32(base, WDT_CTRL_OFF, 0x1u);
    return 0;
}

/* Pet watchdog; must be called at interval p satisfying (1) */
void wdt_pet(volatile void *base) {
    mmio_write32(base, WDT_KICK_OFF, 0xA5A5A5A5u); /* device-specific magic */
}

/* Lock configuration to prevent reconfiguration from less-privileged code */
void wdt_lock_cfg(volatile void *base) {
    mmio_write32(base, WDT_LOCK_OFF, 0xF00DBABEu); /* platform defined */
}