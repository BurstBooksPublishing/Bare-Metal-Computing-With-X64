#include 
#include 

typedef struct {
    uint64_t base;    // start address
    uint64_t len;     // length in bytes
    uint32_t perms;   // bitmask: read/write/exec
    uint32_t region;  // region id
    uint32_t gen;     // generation captured at delegation
    uint8_t  tag;     // hardware tag (1 = valid)
} cap_t;

extern atomic_uint_least32_t current_gen[]; // per-region generation

static inline uint32_t read_pkru(void) {
    uint32_t eax;
    __asm__ volatile("rdpkru" : "=a"(eax) : "c"(0) : "edx");
    return eax;
}
static inline void write_pkru(uint32_t val) {
    uint32_t eax = val;
    __asm__ volatile("wrpkru" : : "a"(eax), "c"(0), "d"(0));
}

// Check capability for access 'req_perm' at address 'addr'
int capability_check(const cap_t *c, void *addr, uint32_t req_perm) {
    uintptr_t a = (uintptr_t)addr;
    // Hardware tag must be present and generation must match
    if (!c->tag) return 0;
    if (c->gen != atomic_load_explicit(¤t_gen[c->region], memory_order_acquire))
        return 0;
    if (a < c->base || a >= c->base + c->len) return 0;
    if ((c->perms & req_perm) == 0) return 0;
    return 1; // allowed
}

// Revoke a region: bump generation, optionally clear tags, and restrict PKRU
void revoke_region(uint32_t region, uint32_t pkru_restrict_mask) {
    // 1) Logical revoke: bump generation so existing caps fail checks
    atomic_fetch_add_explicit(¤t_gen[region], 1u, memory_order_seq_cst);
    // 2) Eager revoke (vendor extension): clear hardware tag bitmap for region
    extern void clear_tag_bitmap(uint32_t region); // provided by firmware/hardware
    clear_tag_bitmap(region);
    // 3) Thread-local immediate protection: set PKRU to restrict pkeys
    uint32_t pkru = read_pkru();
    pkru |= pkru_restrict_mask;
    write_pkru(pkru);
}