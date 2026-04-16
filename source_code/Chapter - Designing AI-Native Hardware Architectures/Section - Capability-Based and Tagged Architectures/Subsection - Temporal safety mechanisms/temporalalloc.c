#include 
#include 
#include 

/* Configuration: tune TAG_BITS and GRANULE_ALIGN to workload. */
#define TAG_BITS 16
#define TAG_SHIFT (64 - TAG_BITS)
#define TAG_MASK (((uint64_t)1 << TAG_BITS) - 1)
#define PTR_MASK (((uint64_t)1 << (64 - TAG_BITS)) - 1)

/* Allocation header placed at object base. */
typedef struct alloc_hdr {
    _Atomic uint16_t gen;   /* generation */
    uint16_t pad;           /* align to 4 bytes */
    uint32_t size;
} alloc_hdr;

/* Simple bump allocator region provided by platform. */
extern void *allocator_region_base;
extern size_t allocator_region_size;
static _Atomic uint64_t allocator_off = 0;
static _Atomic uint16_t global_gen = 1;

/* Compose tagged pointer: high TAG_BITS hold generation. */
static inline uint64_t make_tagged(void *p, uint16_t gen) {
    uint64_t addr = (uint64_t)p & PTR_MASK;
    return ( ((uint64_t)gen << TAG_SHIFT) | addr );
}

/* Extract raw pointer and tag. */
static inline void *raw_ptr(uint64_t tagged) {
    return (void*)(tagged & PTR_MASK);
}
static inline uint16_t ptr_tag(uint64_t tagged) {
    return (uint16_t)(tagged >> TAG_SHIFT);
}

/* Allocate object with header and attached generation. */
uint64_t tagged_malloc(uint32_t size) {
    /* align size and allocate header+payload */
    uint64_t off = atomic_fetch_add(&allocator_off, (uint64_t)sizeof(alloc_hdr) + size);
    if (off + sizeof(alloc_hdr) + size > allocator_region_size) __builtin_trap(); /* OOM */
    uint8_t *base = (uint8_t*)allocator_region_base + off;
    alloc_hdr *h = (alloc_hdr*)base;
    uint16_t gen = atomic_fetch_add(&global_gen, 1) & ((1u<gen, gen);
    h->size = size;
    void *payload = base + sizeof(alloc_hdr);
    return make_tagged(payload, gen);
}

/* Free: increment generation to revoke old pointers. */
void tagged_free(uint64_t tagged) {
    void *p = raw_ptr(tagged);
    alloc_hdr *h = (alloc_hdr*)((uint8_t*)p - sizeof(alloc_hdr));
    /* bump generation atomically to revoke outstanding pointers */
    uint16_t newgen = (atomic_fetch_add(&h->gen, 1) + 1) & ((1u<gen, newgen);
    /* Optionally add block to quarantine to reduce reuse (omitted). */
}

/* Safe dereference: checks generation and returns raw pointer. */
void *deref_checked(uint64_t tagged) {
    void *p = raw_ptr(tagged);
    uint16_t tag = ptr_tag(tagged);
    alloc_hdr *h = (alloc_hdr*)((uint8_t*)p - sizeof(alloc_hdr));
    uint16_t current = atomic_load(&h->gen);
    if (current != tag) __builtin_trap(); /* temporal safety violation */
    return p;
}