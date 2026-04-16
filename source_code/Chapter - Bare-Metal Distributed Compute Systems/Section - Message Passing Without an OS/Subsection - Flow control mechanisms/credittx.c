#include 
#include 
#include 

#define RING_SIZE 1024U
#define DESC_SIZE 64U

/* Shared control area in a cacheline-aligned page (populated by receiver) */
struct peer_ctrl {
    atomic_uint_least32_t credits; /* available credits from receiver */
    atomic_uint_least32_t tx_head; /* sender-owned ring head index */
    uint8_t pad[48];
} __attribute__((aligned(64)));

volatile void * const DOORBELL_MMIO = (volatile void *)0xF0000000ULL; /* example */

/* Memory-mapped descriptor ring (shared) */
struct desc { uint8_t data[DESC_SIZE]; } __attribute__((aligned(64)));
struct desc * const ring = (struct desc *)0x80000000ULL; /* physical mapping */

static inline void mmio_write32(volatile void *addr, uint32_t v) {
    /* ordered volatile write to MMIO register */
    atomic_thread_fence(memory_order_release);
    *(volatile uint32_t *)addr = v;
    /* ensure device sees the write before subsequent reads/writes */
    __asm__ volatile ("sfence" ::: "memory");
}

/* send up to count packets (packet_prepare fills descriptor) */
int send_packets(struct peer_ctrl *ctrl, size_t count,
                 int (*packet_prepare)(struct desc *d, size_t idx, void *ctx),
                 void *ctx)
{
    uint32_t c = atomic_load_explicit(&ctrl->credits, memory_order_acquire);
    if (c == 0) return 0; /* no credits: backpressure to caller */

    size_t to_send = (count < c) ? count : c;
    uint32_t head = atomic_load_explicit(&ctrl->tx_head, memory_order_relaxed);

    for (size_t i = 0; i < to_send; ++i) {
        size_t idx = (head + i) & (RING_SIZE - 1);
        if (!packet_prepare(&ring[idx], idx, ctx)) {
            to_send = i; /* stop on prepare failure */
            break;
        }
    }

    /* publish new head (sender advances ring) */
    uint32_t new_head = (head + to_send) & 0xFFFFFFFFU;
    atomic_store_explicit(&ctrl->tx_head, new_head, memory_order_release);

    /* consume credits atomically */
    atomic_fetch_sub_explicit(&ctrl->credits, (uint32_t)to_send, memory_order_acq_rel);

    /* ring doorbell to NIC (index or byte offset) */
    mmio_write32(DOORBELL_MMIO, new_head);

    return (int)to_send;
}