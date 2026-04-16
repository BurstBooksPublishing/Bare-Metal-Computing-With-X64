#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

#define CACHE_LINE 64
#define RING_SIZE 1024U
_Static_assert((RING_SIZE & (RING_SIZE - 1)) == 0, "RING_SIZE power of two");

struct ring_entry {
    alignas(CACHE_LINE) uint8_t data[64]; // payload fits one cache line
};

struct ring {
    alignas(128) atomic_uint64_t head;
    alignas(128) atomic_uint64_t tail;
    struct ring_entry entries[RING_SIZE];
};

static inline void clwb(void *addr) {
    // x86 CLWB; fallback to CLFLUSHOPT if CLWB unavailable is platform concern.
    asm volatile(".byte 0x66,0x0f,0xae,0x30" /* clwb (%rax) */ 
                 : : "a"(addr) : "memory");
}

static inline void sfence(void) {
    asm volatile("sfence" ::: "memory");
}

void ring_produce(struct ring *r, volatile uint32_t *doorbell_mmio,
                  const void *msg, size_t len) {
    uint64_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint64_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    uint64_t free = (tail - head - 1);
    if (free >= RING_SIZE) {
        // ring full; policy: drop, block, or backoff. Here we spin.
        while (free >= RING_SIZE) {
            tail = atomic_load_explicit(&r->tail, memory_order_acquire);
            free = (tail - head - 1);
        }
    }
    uint32_t idx = (uint32_t)(head & (RING_SIZE - 1));
    // copy payload into ring slot
    struct ring_entry *e = &r->entries[idx];
    // small copy optimized by compiler; ensure whole cache line written
    for (size_t i = 0; i < len && i < sizeof e->data; ++i)
        e->data[i] = ((const uint8_t *)msg)[i];
    // make cache line visible to device
    clwb(e);
    // ensure CLWB ordered and globally visible
    sfence();
    // publish new head with release semantics
    atomic_store_explicit(&r->head, head + 1, memory_order_release);
    // notify device via MMIO doorbell; volatile write orders with respect to device
    *doorbell_mmio = (uint32_t)(head + 1); // posted MMIO write
    // optionally do an mfence here if platform requires stronger ordering
}