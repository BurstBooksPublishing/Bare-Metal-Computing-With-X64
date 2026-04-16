#include 
#include 
#include 

#define CACHELINE 64
#define ALIGN_CL __attribute__((aligned(CACHELINE)))

static inline void clwb_range(void *addr, size_t len) {
    uintptr_t p = (uintptr_t)addr & ~(CACHELINE-1);
    uintptr_t end = (uintptr_t)addr + len;
    for (; p < end; p += CACHELINE)
        asm volatile("clwb (%0)" :: "r"( (void*)p ) : "memory");
    asm volatile("sfence" ::: "memory"); // ensure completion
}
static inline void clflushopt_range(void *addr, size_t len) {
    uintptr_t p = (uintptr_t)addr & ~(CACHELINE-1);
    uintptr_t end = (uintptr_t)addr + len;
    for (; p < end; p += CACHELINE)
        asm volatile("clflushopt (%0)" :: "r"( (void*)p ) : "memory");
    asm volatile("sfence" ::: "memory");
}

typedef struct ALIGN_CL {
    uint8_t *buf;            // contiguous physical or pinned memory
    size_t cap;              // power of two
    size_t mask;
    atomic_uint_fast64_t head; // producer index
    atomic_uint_fast64_t tail; // consumer index
} spsc_ring_t;

// Initialize with pre-allocated physical/pinned buffer.
void spsc_init(spsc_ring_t *r, uint8_t *buf, size_t cap) {
    r->buf = buf;
    r->cap = cap;
    r->mask = cap - 1;
    atomic_init(&r->head, 0);
    atomic_init(&r->tail, 0);
}

// Producer: reserve contiguous space (returns pointer and length available)
uint8_t *spsc_produce_reserve(spsc_ring_t *r, size_t *out_len) {
    uint64_t h = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint64_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    size_t used = (size_t)((h - t) & r->mask);
    size_t free = r->cap - used;
    if (free == 0) { *out_len = 0; return NULL; }
    size_t idx = h & r->mask;
    size_t avail = r->cap - idx;
    *out_len = (avail < free) ? avail : free;
    return &r->buf[idx];
}

// Producer commit: make written bytes visible to device/consumer
void spsc_produce_commit(spsc_ring_t *r, size_t len) {
    uint64_t h = atomic_load_explicit(&r->head, memory_order_relaxed);
    // Writeback to memory before publishing
    clwb_range(&r->buf[h & r->mask], len);
    atomic_store_explicit(&r->head, h + len, memory_order_release);
}

// Consumer: wait or poll for available data and then read
uint8_t *spsc_consume_reserve(spsc_ring_t *r, size_t *out_len) {
    uint64_t t = atomic_load_explicit(&r->tail, memory_order_relaxed);
    uint64_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    size_t used = (size_t)((h - t) & r->mask);
    if (used == 0) { *out_len = 0; return NULL; }
    size_t idx = t & r->mask;
    size_t avail = r->cap - idx;
    *out_len = (avail < used) ? avail : used;
    // Invalidate stale cachelines before reading device-written region
    clflushopt_range(&r->buf[idx], *out_len);
    return &r->buf[idx];
}

void spsc_consume_commit(spsc_ring_t *r, size_t len) {
    uint64_t t = atomic_load_explicit(&r->tail, memory_order_relaxed);
    atomic_store_explicit(&r->tail, t + len, memory_order_release);
}