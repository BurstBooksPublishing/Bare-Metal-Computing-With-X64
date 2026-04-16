#include 
#include 

// Capability descriptor (packed for predictable layout).
struct cap {
    uintptr_t base;   // base address
    size_t    len;    // length in bytes
    uint32_t  perm;   // permission bits (bitfield)
    uint32_t  pkey;   // protection key assigned to pages
    uint32_t  tag;    // optional hardware/software tag
} __attribute__((packed, aligned(8)));

// Read PKRU (returns 32-bit PKRU)
static inline uint32_t rdpkru(void) {
    uint32_t v;
    asm volatile(".byte 0x0f,0x01,0xef" : "=a"(v) : "c"(0) : "memory");
    return v;
}

// Write PKRU (unprivileged if CR4.PKE=1)
static inline void wrpkru(uint32_t v) {
    asm volatile(".byte 0x0f,0x01,0xee" : : "a"(v), "c"(0), "d"(0) : "memory");
}

// Branchless capability check: returns 1 if addr permitted, 0 otherwise.
static inline int cap_check_ct(const struct cap *c, const void *addr, uint32_t want_perm) {
    uintptr_t a = (uintptr_t)addr;
    uintptr_t b = c->base;
    uintptr_t len = c->len;
    // diff1 = a - b; diff2 = b+len-1 - a; high bit set if underflow
    uintptr_t diff1 = a - b;
    uintptr_t diff2 = b + (len - 1) - a;
    uintptr_t high_or = (diff1 | diff2) >> (sizeof(uintptr_t)*8 - 1);
    uint32_t in_range = (~high_or) & 1u;
    // permission subset test: 1 if c->perm includes want_perm
    uint32_t perm_ok = (((c->perm & want_perm) ^ want_perm) == 0);
    // final result (integer 0/1), still branchless for most compilers
    return in_range & perm_ok;
}

// Example revoke: disable both read and write for pkey k (bit pairs per pkey in PKRU)
static inline void revoke_pkey(uint32_t pkey) {
    uint32_t pkru = rdpkru();
    // each pkey uses two bits: AD and WD; setting both to 1 denies access.
    uint32_t mask = 0x3u << (pkey * 2);
    pkru |= mask;
    wrpkru(pkru);
}