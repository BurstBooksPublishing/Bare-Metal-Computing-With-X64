#include 
#include 

/* Capability structure; keep 32-byte alignment for potential tag atomics. */
typedef struct {
    uint64_t base;    /* start address */
    uint64_t len;     /* length in bytes */
    uint16_t perms;   /* bitmask: 1=R,2=W,4=X */
    uint16_t reserved;
    uint64_t tag;     /* 0=invalid, 1=valid */
    uint64_t epoch;   /* version for revocation */
} capability_t;

/* Minimal global epoch table for revocation (simple example). */
volatile uint64_t epoch_table[1024];

/* Create a capability (caller ensures object_id maps to epoch_table). */
static inline capability_t make_cap(uint64_t base, uint64_t len,
                                    uint16_t perms, uint32_t object_id)
{
    capability_t c;
    c.base = base;
    c.len = len;
    c.perms = perms;
    c.reserved = 0;
    c.tag = 1;                      /* mark as valid */
    c.epoch = epoch_table[object_id];/* capture current epoch */
    return c;
}

/* Checked 64-bit read via capability; returns false on violation. */
static inline bool cap_read64(const capability_t *c, uint64_t offset, uint64_t *out)
{
    uint64_t addr = c->base + offset;
    /* Bounds check + permission check + tag/epoch checks */
    if (!c->tag) return false;
    if ((offset >= c->len) || (c->perms & 1) == 0) return false;
    /* Ensure epoch still matches current object epoch (temporal safety) */
    /* Here object_id derivation is domain-specific; assume object id==0 for demo */
    if (c->epoch != epoch_table[0]) return false;
    /* Safe read; use volatile to avoid compiler optimizations across the check */
    *out = *(volatile uint64_t *)(uintptr_t)addr;
    return true;
}