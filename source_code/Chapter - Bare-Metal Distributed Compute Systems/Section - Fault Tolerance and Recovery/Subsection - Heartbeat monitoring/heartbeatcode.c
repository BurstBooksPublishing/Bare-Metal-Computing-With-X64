#include <stdatomic.h>
#include <stdint.h>

#define HB_ADDR ((uint64_t*)0xFFFF000000000000ULL) // physical/MMIO mapped address

// Read serialized TSC using RDTSCP; returns 64-bit tsc.
static inline uint64_t rdtscp64(void) {
    uint32_t lo, hi;
    uint32_t aux;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux) ::);
    return ((uint64_t)hi << 32) | lo;
}

// Sender: bump sequence and publish with release semantics.
void heartbeat_send(void) {
    static atomic_uint_least64_t seq = ATOMIC_VAR_INIT(1);
    uint64_t s = atomic_fetch_add_explicit(&seq, 1, memory_order_relaxed);
    // publish sequence and TSC together if desired (store seq then tsc).
    atomic_store_explicit((atomic_uint_least64_t*)HB_ADDR, s, memory_order_release);
}

// Monitor: check remote heartbeat age in TSC cycles; returns 1 if alive.
int heartbeat_check(uint64_t max_age_cycles) {
    // acquire sequence atomically
    uint64_t s = atomic_load_explicit((atomic_uint_least64_t*)HB_ADDR, memory_order_acquire);
    if (s == 0) return 0; // never initialized
    uint64_t now = rdtscp64();
    // If remote stores timestamp instead of seq, compute age = now - remote_tsc
    // Here we assume mapping of sequence to timestamp is external.
    // For sequence-only, track last_seen and staleness via wall-clock fallback.
    (void)now;
    (void)s;
    // Placeholder: for sequence-based check, a separate remote_tsc location is preferred.
    return 1; // implement staleness test appropriate to mapping used
}