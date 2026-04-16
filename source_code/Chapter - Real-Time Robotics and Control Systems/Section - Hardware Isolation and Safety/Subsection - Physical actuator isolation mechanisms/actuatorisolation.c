#include 
#include  // _mm_mfence

// MMIO physical addresses (example platform-specific values).
#define MOTOR_CTRL_BASE 0xF0000000UL
#define GPIO_BASE       0xF1000000UL

// Offsets (driver-specific).
#define OFF_ENABLE      0x00  // write 1 enables outputs, 0 disables
#define OFF_WATCHDOG    0x04  // write toggle or counter to feed hardware WD
#define OFF_STATUS      0x08  // read-only status bits
#define OFF_ESTOP_IN    0x00  // GPIO input register (bit 0 = E-stop asserted)

// Volatile MMIO accessors.
static inline void mmio_write32(volatile void *addr, uint32_t val) {
    *((volatile uint32_t *)addr) = val;
    _mm_mfence(); // ensure ordering of MMIO on x86
}
static inline uint32_t mmio_read32(volatile void *addr) {
    uint32_t v = *((volatile uint32_t *)addr);
    _mm_mfence();
    return v;
}

// Initialization: disable outputs until system healthy.
void actuator_init(void) {
    mmio_write32((void *)(MOTOR_CTRL_BASE + OFF_ENABLE), 0u);
}

// Feed the hardware watchdog; schedule at rate faster than T_wd.
void feed_watchdog(uint32_t token) {
    mmio_write32((void *)(MOTOR_CTRL_BASE + OFF_WATCHDOG), token);
}

// Assert hard disable immediately; idempotent and atomic.
void hard_disable_actuator(void) {
    mmio_write32((void *)(MOTOR_CTRL_BASE + OFF_ENABLE), 0u);
    // Optionally toggle redundant disable line via GPIO expander.
    mmio_write32((void *)(GPIO_BASE + OFF_ESTOP_IN), 1u); // drive safety line
}

// Poll E-stop and perform immediate hardware isolation if asserted.
void poll_and_isolate(void) {
    uint32_t gpio = mmio_read32((void *)(GPIO_BASE + OFF_ESTOP_IN));
    if (gpio & 0x1u) { // E-stop active
        hard_disable_actuator();
    }
}