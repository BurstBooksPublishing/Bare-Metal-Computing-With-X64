#include 
#include 

#define DESC_RING_SIZE 1024

/* Simple descriptor format: 64-bit phys addr, 32-bit len, 32-bit flags */
struct tx_desc {
    uint64_t paddr;
    uint32_t len;
    uint32_t flags;
} __attribute__((packed));

struct nic {
    volatile struct tx_desc *tx_ring; /* MMIO-allocatable, CPU-writable region */
    volatile uint32_t *tx_tail;       /* MMIO doorbell register */
    uint32_t ring_mask;
};

/* Assume identity-mapped allocator: virt_to_phys is trivial. */
static inline uint64_t virt_to_phys(void *v) { return (uint64_t)v; }

/* Write back cache lines for [buf, buf+len) and serialize. */
static inline void cache_writeback(void *buf, size_t len) {
    volatile char *p = (volatile char*)buf;
    size_t i;
    for (i = 0; i < len; i += 64) {
        asm volatile("clwb (%0)" :: "r"(p + i) : "memory"); /* CLWB for persistent writeback */
    }
    asm volatile("sfence" ::: "memory"); /* Ensure visibility before doorbell */
}

/* Submit a buffer to the NIC ring without copying. */
int nic_tx_zero_copy(struct nic *n, void *buf, size_t len, uint32_t flags) {
    if (len == 0 || len > 0xFFFF) return -1;
    uint64_t paddr = virt_to_phys(buf);

    /* Compute index; NIC expects us to use tail index and update ring. */
    static uint32_t tail_idx = 0;
    uint32_t idx = tail_idx & n->ring_mask;

    /* Ensure data is visible to device. */
    cache_writeback(buf, len);

    /* Populate descriptor atomically (device will read after doorbell). */
    n->tx_ring[idx].paddr = paddr;
    n->tx_ring[idx].len   = (uint32_t)len;
    n->tx_ring[idx].flags = flags;

    /* Notify NIC: write the new tail. The MMIO write acts as a release. */
    tail_idx++;
    *n->tx_tail = tail_idx;

    return 0;
}