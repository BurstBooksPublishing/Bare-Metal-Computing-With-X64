#include 
#include 
#include 
#include  // for _mm_clflushopt, if needed

// Per-core slot aligned to cache line (64 bytes)
typedef struct {
    alignas(64) volatile uint64_t counter; // one 64-bit value per cache line
    // remaining bytes of the line are implicitly padding
} core_slot_t;

static inline uint64_t rdtscp(void) {
    unsigned int aux;
    uint32_t lo, hi;
    __asm__ volatile ("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux) ::);
    return ((uint64_t)hi << 32) | lo;
}

int main(int argc, char **argv) {
    const int iters = 1000000;
    core_slot_t *slots = aligned_alloc(64, 2 * sizeof(core_slot_t));
    if (!slots) return 1;
    slots[0].counter = 0;
    slots[1].counter = 0;

    // Example: single-threaded test mimicking two cores by alternating stores.
    uint64_t t0 = rdtscp();
    for (int i = 0; i < iters; ++i) {
        slots[0].counter++; // core 0 update
        slots[1].counter++; // core 1 update
    }
    uint64_t t1 = rdtscp();
    printf("Cycles per update pair: %.2f\n", (double)(t1 - t0) / iters);

    free(slots);
    return 0;
}