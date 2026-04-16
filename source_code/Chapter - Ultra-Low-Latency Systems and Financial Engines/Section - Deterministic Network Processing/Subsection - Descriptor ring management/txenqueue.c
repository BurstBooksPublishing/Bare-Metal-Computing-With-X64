#include 
#include 

#define CACHE_LINE 64

// DMA descriptor aligned to cache line.
typedef struct __attribute__((aligned(CACHE_LINE))) {
    uint64_t addr;    // physical address for DMA
    uint32_t len;
    uint32_t flags;
} tx_desc_t;

typedef struct {
    tx_desc_t *descs;          // DMA-mapped array (size N)
    uint64_t size;             // N, power of two
    uint64_t mask;             // N-1
    atomic_uint_fast64_t P;    // producer (host)
    atomic_uint_fast64_t C;    // consumer (device-provided)
    volatile uint32_t *doorbell;// MMIO doorbell register pointer
} tx_ring_t;

// Optional cache-line writeback (use CLWB or CLFLUSHOPT if supported).
static inline void writeback_cache_range(void *addr, size_t len) {
    char *p = (char*)addr;
    for (uintptr_t off = 0; off < len; off += CACHE_LINE) {
        asm volatile("clflushopt (%0)" :: "r"(p + off) : "memory"); // fallback if supported
    }
    asm volatile("sfence" ::: "memory");
}

// Ensure MMIO doorbell write is observed by device.
static inline void doorbell_write(volatile uint32_t *db, uint32_t val) {
    *db = val;                     // posted write to device BAR
    asm volatile("" ::: "memory"); // compiler barrier
    (void)*db;                     // readback to stall until write completes
}

int tx_enqueue(tx_ring_t *r, uint64_t dma_addr, uint32_t len, uint32_t flags) {
    uint64_t P = atomic_load_explicit(&r->P, memory_order_relaxed);
    uint64_t C = atomic_load_explicit(&r->C, memory_order_acquire);
    if ((P - C) >= r->size) return -1; // ring full

    uint64_t idx = P & r->mask;
    tx_desc_t *d = &r->descs[idx];

    // Populate descriptor fields (host-only producer).
    d->addr = dma_addr;
    d->len = len;
    d->flags = flags;

    // Make descriptor visible to device.
    writeback_cache_range(d, sizeof(*d));

    // Publish producer index with release semantics.
    atomic_store_explicit(&r->P, P + 1, memory_order_release);

    // Notify NIC via doorbell.
    doorbell_write(r->doorbell, (uint32_t)(P + 1)); // devices often accept lower bits
    return 0;
}