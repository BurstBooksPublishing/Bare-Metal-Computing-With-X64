#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <immintrin.h>

#define CACHELINE 64

/* Element type: replace with application sample structure. */
typedef struct {
    uint64_t timestamp;         /* CPU TSC or hardware timestamp */
    double   values[3];         /* e.g., IMU XYZ or ADC channels */
} sample_t;

/* Ring buffer structure: power-of-two capacity. */
typedef struct {
    alignas(CACHELINE) atomic_uint_least32_t write_idx;
    alignas(CACHELINE) atomic_uint_least32_t read_idx;
    uint32_t capacity;          /* power-of-two */
    uint32_t mask;
    sample_t *buffer;           /* malloc'd or statically allocated region */
} spsc_ring_t;

/* Initialize ring: capacity must be power-of-two. */
static inline void ring_init(spsc_ring_t *r, sample_t *buf, uint32_t capacity) {
    r->buffer = buf;
    r->capacity = capacity;
    r->mask = capacity - 1u;
    atomic_store_explicit(&r->write_idx, 0u, memory_order_relaxed);
    atomic_store_explicit(&r->read_idx, 0u, memory_order_relaxed);
}

/* Producer: push one sample. Safe from IRQ context if allocator avoided. */
static inline bool ring_push(spsc_ring_t *r, const sample_t *s) {
    uint32_t w = atomic_load_explicit(&r->write_idx, memory_order_relaxed);
    uint32_t rd = atomic_load_explicit(&r->read_idx, memory_order_acquire);
    uint32_t next = (w + 1u) & r->mask;
    if (next == rd) return false; /* full */
    r->buffer[w & r->mask] = *s;  /* plain store; ensure sample fits cacheline */
    /* publish write index with release to ensure data visibility */
    atomic_store_explicit(&r->write_idx, next, memory_order_release);
    return true;
}

/* Consumer: pop one sample. Deterministic loop should call this. */
static inline bool ring_pop(spsc_ring_t *r, sample_t *out) {
    uint32_t rd = atomic_load_explicit(&r->read_idx, memory_order_relaxed);
    uint32_t w = atomic_load_explicit(&r->write_idx, memory_order_acquire);
    if (rd == w) return false; /* empty */
    *out = r->buffer[rd & r->mask]; /* plain load */
    /* advance read index with release/relaxed (reader is sole owner) */
    atomic_store_explicit(&r->read_idx, (rd + 1u) & r->mask, memory_order_release);
    return true;
}

/* Occupancy helper */
static inline uint32_t ring_occupancy(const spsc_ring_t *r) {
    uint32_t w = atomic_load_explicit(&r->write_idx, memory_order_acquire);
    uint32_t rd = atomic_load_explicit(&r->read_idx, memory_order_acquire);
    return (w - rd) & r->mask;
}