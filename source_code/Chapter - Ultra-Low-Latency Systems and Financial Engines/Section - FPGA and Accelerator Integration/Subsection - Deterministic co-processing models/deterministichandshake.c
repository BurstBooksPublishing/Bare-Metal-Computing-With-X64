#include 
#include 

volatile uint64_t *const mmioDoorbell = (uint64_t *)0xF0000000; // BAR0 MMIO
volatile uint64_t *const statusRegion = (uint64_t *)0x80000000; // uncached DMA page

// Packed descriptor aligned to cache-line size for deterministic access.
struct Descriptor {
    uint64_t addr;      // device-visible physical address
    uint32_t len;
    uint32_t flags;
} __attribute__((aligned(64)));

volatile struct Descriptor *const ring = (struct Descriptor *)0x81000000; // DMA ring base
const size_t ringSize = 1024; // power-of-two size

static inline uint64_t rdtsc64(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void submitDescriptor(size_t idx, uint64_t physAddr, uint32_t len) {
    volatile struct Descriptor *d = &ring[idx & (ringSize - 1)];
    d->addr = physAddr;          // write descriptor fields
    d->len  = len;
    asm volatile("mfence" ::: "memory"); // ensure descriptor stores visible to device
    *mmioDoorbell = idx;         // doorbell write to notify FPGA
    asm volatile("sfence" ::: "memory"); // ensure MMIO posted write is ordered
}

uint64_t waitCompletion(size_t idx, uint64_t timeoutTsc) {
    uint64_t start = rdtsc64();
    while (1) {
        uint64_t s = statusRegion[idx & (ringSize - 1)]; // uncached read
        if (s != 0) return s;    // nonzero indicates completion token
        if (rdtsc64() - start > timeoutTsc) return 0; // timeout
        // optional PAUSE to reduce power and pipeline stalls
        asm volatile("pause");
    }
}