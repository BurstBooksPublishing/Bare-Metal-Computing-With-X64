#include 
#include 
#include 

/* Replace with actual MMIO doorbell address mapped into this pointer. */
volatile uint32_t * const MMIO_DOORBELL = (volatile uint32_t *)0xF0000000;

/* Simple descriptor struct; match NIC layout in real hardware. */
struct tx_desc {
    uint64_t buf_phys;      // physical address for DMA
    uint32_t len;           // payload length
    uint32_t flags;         // NIC-specific flags
    uint64_t sched_tsc;     // desired transmit TSC (if NIC supports)
};

static inline uint64_t read_tsc(void) {
    unsigned int lo, hi;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    __asm__ volatile("mfence" ::: "memory");
    return ((uint64_t)hi << 32) | lo;
}

/* Publish descriptor with release semantics. */
static inline void publish_desc(struct tx_desc *ring, size_t idx, struct tx_desc *d) {
    atomic_thread_fence(memory_order_release);
    ring[idx] = *d;                      // bulk copy; NIC reads ring memory
    atomic_thread_fence(memory_order_seq_cst);
}

/* Spin until TSC >= target, then ring doorbell once. */
void ring_doorbell_at(uint64_t target_tsc, uint32_t doorbell_value) {
    while (read_tsc() < target_tsc) {
        __asm__ volatile("pause");      // reduce power and contention
    }
    /* Publish a 32-bit doorbell write to notify NIC. */
    *MMIO_DOORBELL = doorbell_value;
    __asm__ volatile("" ::: "memory");  // ensure write ordering
}

/* Schedule a batch of packets starting at start_tsc, spaced by slot_ticks. */
void schedule_batch(struct tx_desc *ring, size_t ring_len,
                    struct tx_desc *batch, size_t batch_len,
                    uint64_t start_tsc, uint64_t slot_ticks,
                    uint32_t doorbell_value_base) {
    for (size_t i = 0; i < batch_len; ++i) {
        size_t idx = (i) % ring_len;
        batch[i].sched_tsc = start_tsc + i * slot_ticks;  // optional
        publish_desc(ring, idx, &batch[i]);
    }
    /* Single doorbell at first scheduled instant for NIC with hardware hold. */
    ring_doorbell_at(start_tsc, doorbell_value_base);
}