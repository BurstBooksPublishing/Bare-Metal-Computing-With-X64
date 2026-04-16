#include 
#include 
#include 

static const uint64_t KERNEL_VIRT_BASE = 0xffff800000000000ULL; // chosen canonical high base
static const uint64_t CANONICAL_MASK = 0xffff000000000000ULL;  // bits 47..63 must match bit47

// Return true if address is canonical per current 48-bit canonical rules.
static inline bool is_canonical(uint64_t v) {
    // Canonical if sign-extension of bit47 equals upper bits.
    uint64_t upper = v & CANONICAL_MASK;
    uint64_t sign = (v >> 47) & 1;
    return (sign ? upper == CANONICAL_MASK : upper == 0);
}

// Convert physical address to higher-half virtual address.
// Caller must ensure p < PHYS_LIMIT required by the mapping.
static inline uint64_t phys_to_virt(uint64_t p) {
    uint64_t v = p + KERNEL_VIRT_BASE;
    assert(is_canonical(v)); // fail early in debug builds if mapping would be non-canonical
    return v;
}

// Convert a higher-half virtual address back to physical.
// Returns UINT64_MAX if address is outside the physmap window.
static inline uint64_t virt_to_phys(uint64_t v) {
    if (!is_canonical(v)) return UINT64_MAX;
    if (v < KERNEL_VIRT_BASE) return UINT64_MAX;
    return v - KERNEL_VIRT_BASE;
}

// Compute PML4 index for a virtual address (bits 47..39).
static inline unsigned pml4_index(uint64_t v) {
    return (unsigned)((v >> 39) & 0x1FF);
}

// Example static assert to ensure PML4 index for kernel base is expected.
_Static_assert(((KERNEL_VIRT_BASE >> 39) & 0x1FF) != 0, "Kernel base PML4 index should be non-zero.");