#include 
#include 
#include    // for _mm_prefetch if available

// Warm a memory region: addr points to mapped, resident memory.
// size: bytes to warm. stride: bytes between touches (use 64).
// prefetch: non-zero to issue software prefetches.
void warm_cache(void *addr, size_t size, size_t stride, int prefetch)
{
    const size_t line = 64;                // typical x64 cache line
    const size_t page = 4096;              // typical page size
    volatile uint8_t *p = (volatile uint8_t *)addr;
    size_t i;

    // Touch one location per page to populate page tables and TLBs.
    for (i = 0; i < size; i += page) {
        volatile uint8_t tmp = p[i];       // volatile read forces access
        (void)tmp;
    }

    // Touch each cache line (or larger stride) to bring lines into L1/L2.
    for (i = 0; i < size; i += stride) {
        if (prefetch) {
            // Hint hardware to prefetch into cache (locality=3 -> T0).
            __builtin_prefetch((const void *)&p[i], 0, 3);
            // fallback: _mm_prefetch((const char*)&p[i], _MM_HINT_T0);
        }
        volatile uint8_t tmp = p[i];       // ensure actual load
        (void)tmp;
    }

    // Ensure all memory operations complete before timing-critical code.
    asm volatile("mfence" ::: "memory");  // serializing fence
}