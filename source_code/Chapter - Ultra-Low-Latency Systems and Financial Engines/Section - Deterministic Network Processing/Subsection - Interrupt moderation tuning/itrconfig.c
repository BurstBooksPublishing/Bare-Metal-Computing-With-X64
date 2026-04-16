#include 
#include 

// bar -- pointer to mapped BAR0 region
// TMR_OFFSET -- device-specific MMIO offset for timer register
// scale -- device-specific conversion factor: reg_value = tau_us * scale
static inline void mmio_write32(volatile void *addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;        /* posted write */
    (void)*(volatile uint32_t *)addr;        /* read-back to flush PCIe write */
    asm volatile ("" ::: "memory");          /* compiler fence */
}

void set_irq_timer(volatile void *bar, size_t TMR_OFFSET, double tau_us, double scale) {
    // Robust bounds check: clamp to device register range
    if (tau_us < 0.0) tau_us = 0.0;
    const double max_tau_us = 1e6; // application-dependent safeguard
    if (tau_us > max_tau_us) tau_us = max_tau_us;

    uint32_t reg = (uint32_t)(tau_us * scale + 0.5); // round to nearest encoding
    mmio_write32((volatile uint8_t *)bar + TMR_OFFSET, reg);
}