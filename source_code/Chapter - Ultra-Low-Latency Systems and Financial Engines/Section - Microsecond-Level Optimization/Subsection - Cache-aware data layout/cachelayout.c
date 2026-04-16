#include 
#include 
#include 
#include  // for _mm_prefetch
#include 

#define CACHE_LINE 64
#define PREFETCH_DISTANCE 16

// prices and sizes are hot-streamed; align to cache line to avoid cross-line fetches.
typedef struct {
    alignas(CACHE_LINE) float *price; // contiguous prices array
    alignas(CACHE_LINE) uint32_t *qty; // contiguous quantities
    size_t n;
} price_soa_t;

// per-core counters padded to cache-line size to prevent false sharing.
typedef struct {
    alignas(CACHE_LINE) uint64_t processed;
    char pad[CACHE_LINE - sizeof(uint64_t)];
} core_counter_t;

// allocate aligned buffer; fall back to aligned_alloc if posix_memalign unavailable.
static inline void *alloc_aligned(size_t bytes) {
    void *p = NULL;
#if defined(_POSIX_C_SOURCE)
    if (posix_memalign(&p, CACHE_LINE, bytes) != 0) return NULL;
#else
    p = aligned_alloc(CACHE_LINE, (bytes + CACHE_LINE - 1) & ~(CACHE_LINE - 1));
#endif
    return p;
}

// initialize SoA with aligned arrays
int init_price_soa(price_soa_t *s, size_t n) {
    s->n = n;
    s->price = (float*)alloc_aligned(n * sizeof(float));
    s->qty   = (uint32_t*)alloc_aligned(n * sizeof(uint32_t));
    if (!s->price || !s->qty) return -1;
    return 0;
}

// processing loop: prefetch future prices to hide memory latency.
void process_ticks(price_soa_t *s, core_counter_t *ctr) {
    size_t n = s->n;
    for (size_t i = 0; i < n; ++i) {
        // prefetch a cache line containing price at i + PREFETCH_DISTANCE
        size_t pf = i + PREFETCH_DISTANCE;
        if (pf < n) _mm_prefetch((const char*)&s->price[pf], _MM_HINT_T0);

        float p = s->price[i];   // hot read in tight loop
        uint32_t q = s->qty[i];
        // lightweight computation that should fit in registers
        (void)(p * (float)q);

        ctr->processed++; // isolated to its own cache line
    }
}