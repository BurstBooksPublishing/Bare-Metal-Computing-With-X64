#include "hal.h"      // board HAL: GPIO, UART, RTC, Flash-OTP, HMAC
#include 

#define KILL_BUTTON_PIN   PIN_BUTTON
#define POWER_GATE_PIN    PIN_PGATE
#define NMI_PIN           PIN_NMI
#define LOG_SLOT          0

// verify_hmac returns 0 on success.
extern int verify_hmac(const uint8_t *msg, size_t msglen,
                       const uint8_t *hmac, size_t hlen);

// atomic_power_off performs final gating with IRQs disabled.
static void atomic_power_off(void) {
    __disable_irq();
    gpio_write(NMI_PIN, 1);       // optionally assert NMI first (edge)
    hal_delay_us(50);             // small propagation bound
    gpio_write(POWER_GATE_PIN, 0); // drive MOSFET gate to cut power
    // No return: power removed or MCU halted by hardware watchdog.
    for (;;) __WFI();
}

int main(void) {
    hal_init();                   // clocks, GPIO, UART, RNG, RTC
    gpio_config_input(KILL_BUTTON_PIN, PULL_UP);
    gpio_config_output(POWER_GATE_PIN, 1); // power on initial state
    gpio_config_output(NMI_PIN, 0);

    // Main loop: UART command or physical button triggers kill.
    for (;;) {
        if (!gpio_read(KILL_BUTTON_PIN)) { // active-low button
            hal_delay_ms(20);            // debounce
            if (!gpio_read(KILL_BUTTON_PIN)) {
                rtc_stamp_event(LOG_SLOT, "BUTTON_PRESS"); // audit log
                atomic_power_off();
            }
        }

        if (uart_available()) {
            uint8_t buf[128], hmac[32];
            size_t n = uart_read_packet(buf, sizeof(buf));
            if (n >= 32) { // last 32 bytes HMAC-SHA256
                size_t mlen = n - 32;
                memcpy(hmac, buf + mlen, 32);
                if (verify_hmac(buf, mlen, hmac, 32) == 0) {
                    rtc_stamp_event(LOG_SLOT, "REMOTE_AUTH");
                    atomic_power_off();
                } else {
                    rtc_stamp_event(LOG_SLOT, "AUTH_FAIL");
                }
            }
        }
        hal_sleep_ms(10); // low-power polling
    }
    return 0;
}