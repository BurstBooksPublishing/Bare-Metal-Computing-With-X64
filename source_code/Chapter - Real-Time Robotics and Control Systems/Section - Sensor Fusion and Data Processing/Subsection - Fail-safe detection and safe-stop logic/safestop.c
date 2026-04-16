#include 
#include 

// Hardware-specific addresses (set per platform).
#define PWM_BASE_ADDR   0xF0000000U
#define BRAKE_GPIO_ADDR 0xF0001000U
#define WATCHDOG_ADDR   0xF0002000U

// Volatile memory-mapped registers.
volatile uint32_t * const pwm = (uint32_t *)PWM_BASE_ADDR;
volatile uint32_t * const brake = (uint32_t *)BRAKE_GPIO_ADDR;
volatile uint32_t * const wdog = (uint32_t *)WATCHDOG_ADDR;

// Atomic guard to ensure single execution and idempotence.
static atomic_flag failsafe_flag = ATOMIC_FLAG_INIT;

// Write barrier to ensure ordered MMIO.
static inline void mmio_fence(void){
    asm volatile("mfence" ::: "memory");
}

// Primary safe-stop sequence. Must be small, deterministic, and idempotent.
void safe_stop(void){
    // If already in safe state, return quickly.
    if (atomic_flag_test_and_set(&failsafe_flag)) return;

    // Disable maskable interrupts immediately.
    asm volatile("cli" ::: "memory");

    // Ensure prior writes complete before asserting hardware.
    mmio_fence();

    // Zero PWM outputs (assumes contiguous channels; write safely).
    // Loop count tailored to hardware PWM channel count; keep deterministic.
    for (int i = 0; i < 8; ++i) { // adjust 8 to actual channel count
        pwm[i] = 0;             // disable drive effort
    }
    mmio_fence();

    // Assert mechanical brake line (active-high assumed).
    *brake = 1;
    mmio_fence();

    // Configure watchdog to enforce hardware reset if system stalls.
    // Writing a specific key and timeout is platform-dependent.
    *wdog = 0xA5;   // example: kick/arm watchdog (replace with platform API)
    mmio_fence();

    // Optionally de-energize motor driver enable line here (omitted).
    // Enter an infinite safe loop. Use pause to reduce power and avoid spinlock noise.
    while (1) {
        asm volatile("pause");
    }
}