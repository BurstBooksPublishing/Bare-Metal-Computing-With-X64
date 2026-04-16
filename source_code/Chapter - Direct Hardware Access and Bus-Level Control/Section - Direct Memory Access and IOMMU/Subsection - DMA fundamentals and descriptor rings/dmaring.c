#include 
#include 
#include  // for _mm_sfence on x86-64

// Descriptor layout for device (packed, hardware-defined).
struct dma_desc {
    uint64_t buf_phys;    // physical address of payload
    uint32_t len;         // length in bytes
    uint32_t flags;       // host-owned, EOP, checksum, etc.
} __attribute__((packed, aligned(16)));

// Ring control structure (host-only)
struct dma_ring_host {
    struct dma_desc *ring;    // virtual pointer to descriptors
    uint64_t ring_phys;       // physical base for device use
    uint32_t size;            // N, power-of-two
    uint32_t mask;            // N-1
    uint32_t head;            // producer index (host)
    volatile uint32_t *tail_mmio; // device tail/doorbell (MMIO)
};

// Flush descriptor cache lines so device sees updates.
static inline void clflush_range(void *addr, size_t len) {
    uintptr_t p = (uintptr_t)addr;
    uintptr_t end = p + len;
    for (; p < end; p += 64) {
        asm volatile("clflushopt (%0)" :: "r"( (void*)p) : "memory");
    }
    _mm_sfence(); // ensure ordering of flushes
}

// Submit one buffer to ring; returns 0 on success, -1 if ring full.
int ring_submit(struct dma_ring_host *r, uint64_t buf_phys, uint32_t len, uint32_t flags) {
    uint32_t next = (r->head + 1) & r->mask;
    // Read device consumer index via memory-mapped doorbell protocol if provided.
    // For simple hardware, device writes a tail value elsewhere; host tracks via completion.
    // Here we assume host can detect fullness via reserved slot rule.
    // Compute free slots conservatively by reading tail from device if available.
    // If no tail read, higher-level flow must ensure space.
    // For brevity, perform simple fullness check using reserved-slot convention:
    extern uint32_t device_consumer_index_get(void); // platform-defined
    uint32_t dev_tail = device_consumer_index_get();
    uint32_t occupied = (r->head - dev_tail + r->size) & r->mask;
    if (occupied == r->mask) return -1; // ring full

    struct dma_desc *d = &r->ring[r->head];
    d->buf_phys = buf_phys;
    d->len = len;
    d->flags = flags | (1u << 31); // set host-owned bit conventionally high-bit

    // Ensure descriptor writes reach memory and are visible to device.
    clflush_range(d, sizeof(*d));
    // Memory barrier to order descriptor writes before doorbell.
    asm volatile("sfence" ::: "memory");

    // Notify device by writing new head/tail register (doorbell).
    // Device interprets doorbell as release; write must be a non-temporal MMIO store.
    *r->tail_mmio = (r->head + 1) & 0xFFFFFFFFu;
    r->head = next;
    return 0;
}