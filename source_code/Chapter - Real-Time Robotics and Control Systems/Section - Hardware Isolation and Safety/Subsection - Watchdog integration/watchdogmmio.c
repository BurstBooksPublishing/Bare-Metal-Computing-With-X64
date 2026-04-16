/* Minimal, production-quality MMIO watchdog driver for bare-metal x64. */
/* Adapt WDOG_BASE, UNLOCK_KEY, and CONTROL flags to target hardware. */

#include 
#include 

#define WDOG_BASE     ((uintptr_t)0xFEC00000) /* platform-specific base */
#define UNLOCK_KEY    0x1ACCE551U            /* example magic unlock */
#define CTRL_ENABLE   (1U << 0)
#define CTRL_RESET_ON_TIMEOUT (1U << 1)

typedef volatile uint32_t v32;
struct wdog_regs {
    v32 LOAD;    /* 0x00 */
    v32 VALUE;   /* 0x04 (readback) */
    v32 CONTROL; /* 0x08 */
    v32 KICK;    /* 0x0C (write-only pet register) */
    v32 LOCK;    /* 0x10 (write unlock sequence) */
};

static inline void mmio_wbarrier(void) { asm volatile("" ::: "memory"); } /* compiler barrier */

static inline struct wdog_regs *wdog(void) {
    return (struct wdog_regs *)WDOG_BASE;
}

/* Unlock watchdog for configuration (platform may require sequence). */
static inline void watchdog_unlock(void) {
    wdog()->LOCK = UNLOCK_KEY;
    mmio_wbarrier();
}

/* Lock watchdog to prevent accidental writes. */
static inline void watchdog_lock(void) {
    wdog()->LOCK = 0;
    mmio_wbarrier();
}

/* Initialize watchdog with tick count N and enable or disable reset on timeout. */
static void watchdog_init(uint32_t ticks, int enable_reset) {
    watchdog_unlock();
    wdog()->LOAD = ticks;          /* set timeout count */
    mmio_wbarrier();
    uint32_t ctrl = CTRL_ENABLE | (enable_reset ? CTRL_RESET_ON_TIMEOUT : 0);
    wdog()->CONTROL = ctrl;
    mmio_wbarrier();
    watchdog_lock();
}

/* Pet the watchdog; inexpensive and safe to call from high-priority context. */
static inline void watchdog_kick(void) {
    /* Some hardware requires a specific pattern; use KICK register. */
    wdog()->KICK = 0xA5A5A5A5U;
    mmio_wbarrier();
}

/* Disable watchdog (use only during controlled shutdown). */
static void watchdog_disable(void) {
    watchdog_unlock();
    wdog()->CONTROL = 0;
    mmio_wbarrier();
    watchdog_lock();
}

/* Example usage in a deterministic control loop (call from highest-priority task):
   - compute control output, update actuators;
   - immediately call watchdog_kick() before lower-priority work. */