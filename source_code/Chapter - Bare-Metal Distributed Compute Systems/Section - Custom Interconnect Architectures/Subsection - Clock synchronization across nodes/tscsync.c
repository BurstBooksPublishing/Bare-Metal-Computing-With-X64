#include <stdint.h>

// Read TSC (portable inline asm for x64).
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile ("lfence\nrdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((uint64_t)hi << 32) | lo;
}

// Platform: send raw frame containing t1; returns 0 on success.
extern int nic_send_frame(const void *buf, size_t len);

// Platform: receive raw frame into buf; returns length or -1 on timeout.
extern int nic_receive_frame(void *buf, size_t maxlen, uint32_t timeout_ms);

typedef struct { double tsc_freq_hz; double offset_ns; double alpha; } sync_state_t;

// Frame payload layout: client_t1 (uint64_t), server_t2 (uint64_t), server_t3 (uint64_t)
int sync_once(sync_state_t *st, const uint8_t *dst_mac) {
    uint8_t tx_buf[1500] = {0}, rx_buf[1500];
    uint64_t t1, t4, t2, t3;
    // build minimal ethernet/IP-less frame; application must set dst_mac, ethertype, etc.
    // (omitted: frame headers) Place client timestamp at payload offset.
    t1 = rdtsc();
    *(uint64_t*)(tx_buf + 64) = t1; // payload offset chosen to align with NIC DMA
    if (nic_send_frame(tx_buf, 128) != 0) return -1; // send request

    // wait for reply; capture t4 as close to arrival as possible
    int len = nic_receive_frame(rx_buf, sizeof(rx_buf), 100);
    if (len <= 0) return -2;
    t4 = rdtsc();
    // extract server timestamps (already stamped in server firmware)
    t2 = *(uint64_t*)(rx_buf + 64);
    t3 = *(uint64_t*)(rx_buf + 72);

    // compute offset and delay in ticks (eq. (1))
    int64_t theta_ticks = ((int64_t)(t2 - t1) + (int64_t)(t3 - t_4)) / 2;
    int64_t delay_ticks = (int64_t)(t4 - t1) - (int64_t)(t3 - t2);

    // convert to nanoseconds
    double ticks_to_ns = 1e9 / st->tsc_freq_hz;
    double measured_offset_ns = theta_ticks * ticks_to_ns;
    double measured_delay_ns  = delay_ticks  * ticks_to_ns;

    // exponential smoothing (eq. (3))
    st->offset_ns += st->alpha * (measured_offset_ns - st->offset_ns);

    // Optionally return measured delay for monitoring
    (void)measured_delay_ns;
    return 0;
}