#include <stdint.h>
#include <stddef.h>

/* Read serialized TSC via RDTSCP (returns nanoseconds using known tsc_freq_hz). */
static inline uint64_t rdtscp_raw(void) {
    unsigned int aux;
    uint64_t rax, rdx;
    __asm__ __volatile__("rdtscp" : "=a"(rax), "=d"(rdx), "=c"(aux) ::);
    return (rdx << 32) | rax;
}

/* Convert TSC ticks to nanoseconds using tsc_freq_hz determined at startup. */
static inline uint64_t tsc_to_ns(uint64_t tsc, double tsc_freq_hz) {
    return (uint64_t)((tsc / tsc_freq_hz) * 1e9);
}

/* MMIO read 64-bit counter at offset (volatile pointer to mapped region). */
static inline uint64_t mmio_read64(volatile void *base, uintptr_t offset) {
    return *(volatile uint64_t *)((uintptr_t)base + offset);
}

/* Structure holding affine calibration parameters. */
struct affine {
    double a; /* ns per device tick */
    double b; /* ns offset */
};

/* Calibrate using two samples separated by a controlled delay. */
void calibrate_affine(struct affine *out, volatile void *mmio_base,
                      uintptr_t counter_off, double tsc_freq_hz) {
    uint64_t c1 = mmio_read64(mmio_base, counter_off);
    uint64_t t1 = tsc_to_ns(rdtscp_raw(), tsc_freq_hz);
    /* Wait a short deterministic interval; implement busy-wait or timer. */
    for (volatile int i = 0; i < 1000000; ++i) __asm__ __volatile__("");
    uint64_t c2 = mmio_read64(mmio_base, counter_off);
    uint64_t t2 = tsc_to_ns(rdtscp_raw(), tsc_freq_hz);

    out->a = (double)(t2 - t1) / (double)(c2 - c1);
    out->b = (double)t1 - out->a * (double)c1;
}

/* Convert a device counter reading to host nanoseconds. */
static inline uint64_t dev_to_host_ns(const struct affine *cfg, uint64_t c) {
    return (uint64_t)(cfg->a * (double)c + cfg->b + 0.5);
}

/* Example usage:
   - mmio_base obtained via PCI BAR mapping.
   - tsc_freq_hz measured once via calibration.
*/