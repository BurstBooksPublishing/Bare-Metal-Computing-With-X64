#include 
#include 
#include  // for _mm_clflushopt, _mm_sfence

#define CACHELINE 64
#define ALIGN_CACHE __attribute__((aligned(CACHELINE)))

typedef struct ALIGN_CACHE {
    atomic_uint_fast64_t head; // consumer cursor (separate cacheline)
    char pad1[CACHELINE - sizeof(atomic_uint_fast64_t)];
    atomic_uint_fast64_t tail; // producer cursor (separate cacheline)
    char pad2[CACHELINE - sizeof(atomic_uint_fast64_t)];
    uint8_t *slots;           // pointer to N * slot_size bytes
    uint64_t slot_mask;       // N - 1, requires N power-of-two
    uint64_t slot_size;
} ring_t;

/* flush a memory range of length len starting at p to make DMA-safe */
static inline void flush_range(void *p, size_t len) {
    uintptr_t addr = (uintptr_t)p & ~(CACHELINE - 1);
    uintptr_t end = (uintptr_t)p + len;
    for (; addr < end; addr += CACHELINE)
        _mm_clflushopt((void*)addr); // non-temporal flush if available
    _mm_sfence(); // ensure write-back completes before device sees memory
}

/* Producer: enqueue single slot (returns 0 on success, -1 on full) */
static inline int ring_enqueue(ring_t *r, const void *src) {
    uint64_t t = atomic_load_explicit(&r->tail, memory_order_relaxed);
    uint64_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    if ((t - h) >= (r->slot_mask + 1)) return -1; // full
    uint64_t idx = t & r->slot_mask;
    uint8_t *dst = r->slots + idx * r->slot_size;
    // copy message into slot (must be cache-line aligned or fully flushed)
    for (uint64_t i = 0; i < r->slot_size; i += sizeof(uint64_t))
        *(uint64_t*)(dst + i) = *(const uint64_t*)((const uint8_t*)src + i);
    // Ensure memory visible to peer/DMA before publishing tail
    flush_range(dst, r->slot_size);
    // release store publishes the new tail
    atomic_store_explicit(&r->tail, t + 1, memory_order_release);
    return 0;
}

/* Consumer: dequeue single slot (returns 0 on success, -1 on empty) */
static inline int ring_dequeue(ring_t *r, void *dst) {
    uint64_t h = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint64_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    if (h == t) return -1; // empty
    uint64_t idx = h & r->slot_mask;
    uint8_t *src = r->slots + idx * r->slot_size;
    // read message
    for (uint64_t i = 0; i < r->slot_size; i += sizeof(uint64_t))
        *(uint64_t*)((uint8_t*)dst + i) = *(uint64_t*)(src + i);
    // release store indicates slot is free
    atomic_store_explicit(&r->head, h + 1, memory_order_release);
    return 0;
}