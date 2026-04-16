#include 
#include 

/* MMIO access helpers */
static inline void mmio_write32(volatile void *addr, uint32_t v) {
    __asm__ volatile ("" ::: "memory");         // compiler barrier
    *(volatile uint32_t *)addr = v;
    __asm__ volatile ("sfence" ::: "memory");   // ensure device sees write
}

/* Descriptor layout — match NIC hardware spec */
struct tx_desc {
    uint64_t src_phys;    // local physical address
    uint64_t dst_phys;    // remote physical address
    uint32_t length;
    uint32_t lkey;        // local key representing MR
    uint32_t rkey;        // remote key supplied by peer
    uint32_t opcode;      // RDMA_WRITE, etc.
};

/* Ring state kept in host memory, shared with device */
struct tx_ring {
    struct tx_desc *descs;    // physically contiguous array
    uint32_t size;            // power-of-two
    uint32_t head;            // software head
    uint32_t tail;            // device head (shadow)
    uintptr_t doorbell_addr;  // MMIO doorbell register
};

/* Post a single RDMA write work request */
int rdma_post_write(struct tx_ring *ring,
                    uint64_t local_phys, uint64_t remote_phys,
                    uint32_t len, uint32_t lkey, uint32_t rkey) {
    uint32_t head = ring->head;
    uint32_t idx = head & (ring->size - 1);
    struct tx_desc *d = &ring->descs[idx];

    /* Ensure there is space: device consumes until tail != head+1 */
    uint32_t next = head + 1;
    if (next - ring->tail >= ring->size) return -1; // ring full

    /* Populate descriptor (ensure writes reach memory before doorbell) */
    d->src_phys = local_phys;
    d->dst_phys = remote_phys;
    d->length = len;
    d->lkey = lkey;
    d->rkey = rkey;
    d->opcode = 1; // 1 == RDMA_WRITE (vendor-specific)

    /* Commit descriptor to memory, then advance head */
    __asm__ volatile ("sfence" ::: "memory");
    ring->head = next;

    /* Ring doorbell: write new head index to device MMIO */
    mmio_write32((void *)ring->doorbell_addr, next);

    return 0;
}