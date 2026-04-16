#include 
#include 

/* Parameters: adjust for your platform. */
#define TAG_GRANULARITY 16u
#define TAG_SHIFT 4u /* log2(TAG_GRANULARITY) */

/* Tag store base must be page-aligned and cover RAM; provision at boot. */
extern uint8_t *tag_store_base; /* mapped to physical tag storage */

/* Compute index for byte address. */
static inline size_t tag_index(uintptr_t addr) {
    return addr >> TAG_SHIFT;
}

/* Atomically read tag for an address. */
static inline uint8_t tag_get(uintptr_t addr) {
    size_t idx = tag_index(addr);
    return atomic_load_explicit((atomic_uint8_t *)&tag_store_base[idx],
                                memory_order_acquire);
}

/* Atomically set tag, return old tag. */
static inline uint8_t tag_set(uintptr_t addr, uint8_t new_tag) {
    size_t idx = tag_index(addr);
    return atomic_exchange_explicit((atomic_uint8_t *)&tag_store_base[idx],
                                    new_tag, memory_order_acq_rel);
}

/* Check tag and perform a 64-bit load if allowed; otherwise call fault handler. */
static inline uint64_t tagged_load64(uintptr_t addr, uint8_t expected_tag,
                                     void (*fault_handler)(uintptr_t, uint8_t)) {
    uint8_t t = tag_get(addr);
    if (t != expected_tag) {
        fault_handler(addr, t); /* platform-specific synchronous fault. */
        __builtin_unreachable();
    }
    /* Unaligned access may be platform-defined; here we assume aligned. */
    return *(volatile uint64_t *)addr; /* direct load after tag check */
}