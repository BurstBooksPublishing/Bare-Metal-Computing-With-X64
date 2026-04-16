#include 
#include 

#define REG_STATUS 0x00U     // status: bit0 FIFO non-empty, bit1 overflow
#define REG_FIFO   0x04U     // FIFO read (32-bit words)
#define MAX_SAMPLES  (1<<20) // max host buffer entries

volatile uint8_t *mmio_base = (volatile uint8_t *)0xF8000000; // mapped BAR base

static inline unsigned long long rdtsc(void) {
    unsigned hi, lo;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

void drain_trace(void) {
    uint64_t out_ts[MAX_SAMPLES];
    uint32_t out_ev[MAX_SAMPLES];
    size_t idx = 0;

    while (idx < MAX_SAMPLES) {
        uint32_t status = *(volatile uint32_t *)(mmio_base + REG_STATUS);
        if ((status & 1U) == 0) continue; // FIFO empty, busy-wait (polling)
        uint32_t ev = *(volatile uint32_t *)(mmio_base + REG_FIFO); // read event
        unsigned long long t = rdtsc(); // timestamp as close as possible to read
        out_ev[idx] = ev;
        out_ts[idx] = t;
        idx++;
        // optional: check overflow flag and log once
        if (status & 2U) {
            // overflow detected; handle according to experiment policy
        }
    }

    // post-process: write buffer to storage or analyze in-place
}