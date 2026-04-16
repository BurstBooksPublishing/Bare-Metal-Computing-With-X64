#include 

// f_tsc in Hz, nic_bar points to mapped MMIO region.
double f_tsc;
volatile uint64_t *nic_bar; // mapped ahead of use

static inline uint64_t rdtscp_serial(void) {
    uint32_t aux;
    uint64_t t;
    asm volatile("rdtscp\n\t"          // serializing read, returns EDX:EAX
                 "shl $32, %%rdx\n\t"
                 "or %%rdx, %%rax\n\t"
                 : "=a"(t), "=d"(aux) :: "rcx");
    return t;
}

double cycles_to_seconds(uint64_t cycles) {
    return (double)cycles / f_tsc;
}

// Read NIC timestamp at RX descriptor index idx (example offset computation).
static inline uint64_t nic_read_rx_ts(size_t idx) {
    size_t offset = /* computed per NIC layout */ idx * 16 + 0x200; // example
    return *(volatile uint64_t *)((uintptr_t)nic_bar + offset);
}

// Read NIC egress timestamp from TX completion entry.
static inline uint64_t nic_read_tx_ts(size_t idx) {
    size_t offset = /* computed per NIC layout */ idx * 16 + 0x400; // example
    return *(volatile uint64_t *)((uintptr_t)nic_bar + offset);
}

// Capture tick-to-trade for one message path.
int capture_tick_to_trade(size_t rx_idx, size_t tx_idx, double *latency_sec) {
    uint64_t t_ing = nic_read_rx_ts(rx_idx);         // NIC ingress HW timestamp
    uint64_t t_dec = rdtscp_serial();                // CPU decision timestamp
    // Application forms order here (deterministic code path).
    // Enqueue TX descriptor and notify NIC (omitted).
    uint64_t t_eg = nic_read_tx_ts(tx_idx);          // NIC egress HW timestamp
    // If NIC timestamps use different epoch, apply offset correction here.
    uint64_t cycles = (t_eg > t_ing) ? (t_eg - t_ing) : 0;
    *latency_sec = cycles_to_seconds(cycles);
    return 0;
}