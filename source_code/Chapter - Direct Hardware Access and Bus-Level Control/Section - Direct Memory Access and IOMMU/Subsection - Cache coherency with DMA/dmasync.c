#include 
#include 

/* Cache line size assumed; replace with runtime query if available. */
#define CACHELINE 64

/* Write back CPU-modified lines so device reads observe updates. */
static inline void dma_sync_for_device(void *addr, size_t len) {
    uintptr_t start = (uintptr_t)addr & ~(CACHELINE - 1);
    uintptr_t end = ((uintptr_t)addr + len + CACHELINE - 1) & ~(CACHELINE - 1);
    if (__builtin_cpu_supports("clwb")) {
        for (uintptr_t p = start; p < end; p += CACHELINE)
            asm volatile("clwb (%0)" :: "r"((void*)p) : "memory");
        asm volatile("sfence" ::: "memory"); /* ensure persistence ordering */
    } else if (__builtin_cpu_supports("clflushopt")) {
        for (uintptr_t p = start; p < end; p += CACHELINE)
            asm volatile("clflushopt (%0)" :: "r"((void*)p) : "memory");
        asm volatile("sfence" ::: "memory");
    } else {
        for (uintptr_t p = start; p < end; p += CACHELINE)
            asm volatile("clflush (%0)" :: "r"((void*)p) : "memory");
        asm volatile("mfence" ::: "memory"); /* conservative fallback */
    }
}

/* Invalidate CPU cache lines so CPU reads observe device DMA writes. */
static inline void dma_sync_for_cpu(void *addr, size_t len) {
    uintptr_t start = (uintptr_t)addr & ~(CACHELINE - 1);
    uintptr_t end = ((uintptr_t)addr + len + CACHELINE - 1) & ~(CACHELINE - 1);
    if (__builtin_cpu_supports("clflushopt")) {
        for (uintptr_t p = start; p < end; p += CACHELINE)
            asm volatile("clflushopt (%0)" :: "r"((void*)p) : "memory");
        asm volatile("sfence" ::: "memory"); /* ensure invalidation completed */
    } else {
        for (uintptr_t p = start; p < end; p += CACHELINE)
            asm volatile("clflush (%0)" :: "r"((void*)p) : "memory");
        asm volatile("mfence" ::: "memory");
    }
}