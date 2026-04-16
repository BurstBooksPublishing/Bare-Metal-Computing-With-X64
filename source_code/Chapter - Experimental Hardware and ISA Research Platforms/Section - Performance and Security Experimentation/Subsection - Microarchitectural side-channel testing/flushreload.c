#include 
#include 

/* Measure load latency to addr. Returns cycle delta. */
static inline uint64_t flush_reload_probe(const void *addr) {
    unsigned int aux;
    uint64_t t0 = __rdtsc();                          /* start timestamp */
    volatile unsigned char v = *(volatile unsigned char *)addr; /* load */
    uint64_t t1 = __rdtscp(&aux);                     /* serializing end */
    _mm_clflush(addr);                                /* evict line */
    _mm_mfence();                                     /* ensure clflush completes */
    (void)v;                                          /* prevent compiler opt */
    return t1 - t0;
}

/* Usage note: pin thread to core, disable sibling SMT, and mask interrupts. */