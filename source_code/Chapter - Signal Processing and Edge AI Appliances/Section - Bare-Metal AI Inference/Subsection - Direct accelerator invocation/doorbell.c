#include 
#include 

#define BAR0_BASE ((volatile uint8_t*)0xF0000000ULL) /* mapped device BAR0 base */
#define DOORBELL_OFFSET 0x0100
#define HEAD_OFFSET     0x0108
#define RING_SIZE       1024
#define CACHE_LINE_SIZE 64

/* Minimal descriptor aligned to cache-line boundaries */
struct __attribute__((aligned(64))) descriptor {
    uint64_t phys_addr; /* device-visible physical address */
    uint32_t length;
    uint32_t flags;
    uint8_t  ctx[40];
};

volatile struct descriptor *ring = (volatile struct descriptor*)0x80000000ULL; /* physical, identity mapped */
volatile uint32_t *host_tail = (volatile uint32_t*)0x80010000ULL; /* per-host ring state */

/* write MMIO 32-bit */
static inline void mmio_write32(volatile void *addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
    asm volatile ("" ::: "memory"); /* prevent compiler reorder */
}

/* cache line write-back (clwb if supported, else clflushopt) */
static inline void cache_writeback(void *addr) {
    asm volatile ("clwb (%0)" :: "r"(addr) : "memory"); /* safe on modern CPUs */
}

/* store fence to ensure write-back completes before doorbell */
static inline void store_fence(void) {
    asm volatile ("sfence" ::: "memory");
}

/* Submit one descriptor and wait for completion indicated by device head advancing. */
int submit_descriptor_and_wait(uint32_t ring_index, uint64_t phys_payload, uint32_t len, uint32_t flags) {
    volatile struct descriptor *d = &ring[ring_index % RING_SIZE];

    /* 1) Populate descriptor fields */
    d->phys_addr = phys_payload;
    d->length = len;
    d->flags = flags;

    /* 2) Write-back cache lines touched by the descriptor */
    for (uintptr_t p = (uintptr_t)d; p < (uintptr_t)d + sizeof(*d); p += CACHE_LINE_SIZE)
        cache_writeback((void*)p);

    /* 3) Ensure write-backs are ordered and visible to device */
    store_fence();

    /* 4) Publish tail via MMIO doorbell (device reads new tail and scans ring) */
    mmio_write32((void*)(BAR0_BASE + DOORBELL_OFFSET), ring_index);

    /* 5) Poll device head for completion deterministically */
    uint32_t max_spin = 1000000;
    while (max_spin--) {
        uint32_t head = *(volatile uint32_t*)(BAR0_BASE + HEAD_OFFSET);
        if (head == ring_index) return 0; /* completed */
    }
    return -1; /* timeout */
}