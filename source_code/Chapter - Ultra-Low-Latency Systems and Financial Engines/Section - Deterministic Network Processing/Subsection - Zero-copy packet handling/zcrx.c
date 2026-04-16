#include 
#include 

#define CACHELINE 64
#define RING_SIZE 1024
#define DESC_OWNED_BY_NIC 0x1

/* MMIO doorbell/write helpers (platform-specific) */
static inline void mmio_write32(volatile uint32_t *addr, uint32_t v) {
    *addr = v;                /* MMIO write */
    asm volatile("sfence" ::: "memory"); /* ensure ordering to device */
}
static inline uint32_t mmio_read32(volatile uint32_t *addr){
    uint32_t v = *addr;
    asm volatile("" ::: "memory"); /* compiler barrier */
    return v;
}

/* Descriptor visible to both NIC and CPU */
struct rx_desc {
    uint64_t buf_addr;       /* physical address for DMA */
    uint16_t length;         /* filled by NIC */
    uint16_t flags;          /* offload flags, etc. */
    volatile uint8_t status; /* ownership/status bits */
    uint8_t pad[7];          /* align to 32 bytes */
} __attribute__((packed, aligned(32)));

struct rx_desc *rx_ring;               /* ring base (phys pinned) */
atomic_uint_fast32_t cpu_head = 0;     /* index CPU consumes */
volatile uint32_t *nic_tail_mmio;      /* NIC tail register (MMIO) */

/* Consumer ring for passing pointers lock-free (SPSC) */
void *consumer_ring[RING_SIZE];
atomic_uint_fast32_t consumer_tail = 0;

void rx_poll_once(void) {
    uint32_t idx = atomic_load_explicit(&cpu_head, memory_order_relaxed);
    struct rx_desc *d = &rx_ring[idx];
    /* Read status with acquire semantics to observe NIC writes */
    uint8_t status = d->status; /* volatile read */
    if (status & DESC_OWNED_BY_NIC) return; /* no new packet */

    /* Ensure descriptor fields (length, buf_addr) are observed */
    asm volatile("lfence" ::: "memory");

    /* Zero-copy handoff: publish buffer pointer to consumer ring */
    void *pkt = (void *)(uintptr_t)d->buf_addr;
    uint32_t cons_idx = atomic_load_explicit(&consumer_tail, memory_order_relaxed);
    consumer_ring[cons_idx % RING_SIZE] = pkt;
    /* advance consumer tail with release to make pkt visible */
    atomic_store_explicit(&consumer_tail, cons_idx + 1, memory_order_release);

    /* Re-arm descriptor for NIC: clear status and set ownership bit */
    d->status = DESC_OWNED_BY_NIC;
    /* ensure writes reach memory before notifying NIC */
    asm volatile("sfence" ::: "memory");

    /* Notify NIC by updating tail/doorbell */
    mmio_write32(nic_tail_mmio, idx);

    /* advance cpu_head */
    atomic_store_explicit(&cpu_head, (idx + 1) % RING_SIZE, memory_order_relaxed);
}