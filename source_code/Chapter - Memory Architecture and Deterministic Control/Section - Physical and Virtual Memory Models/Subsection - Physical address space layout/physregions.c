#include 
#include 
#include 
#include 

typedef enum { REGION_RAM, REGION_RESERVED, REGION_MMIO, REGION_ROM } region_type_t;

typedef struct {
    uint64_t start;
    uint64_t size;
    region_type_t type;
} phys_region_t;

typedef struct {
    phys_region_t *entries;
    size_t count;
    size_t cap;
} phys_map_t;

static inline uint64_t align_up_u64(uint64_t x, uint64_t a) {
    assert((a & (a - 1)) == 0); // power-of-two alignment
    return (x + (a - 1)) & ~(a - 1);
}

static void map_init(phys_map_t *m, size_t cap) {
    m->entries = malloc(cap * sizeof(phys_region_t));
    m->count = 0;
    m->cap = cap;
}

static void map_add(phys_map_t *m, uint64_t start, uint64_t size, region_type_t t) {
    assert(size != 0);
    assert(m->count < m->cap);
    // simple append; caller should canonicalize/merge if needed
    m->entries[m->count++] = (phys_region_t){ .start = start, .size = size, .type = t };
}

static bool region_overlaps(const phys_region_t *a, const phys_region_t *b) {
    return !(a->start + a->size <= b->start || b->start + b->size <= a->start);
}

static bool find_free_region(phys_map_t *m, uint64_t size, uint64_t align,
                             uint64_t min_addr, uint64_t *out_base) {
    // scan RAM regions and attempt to place an aligned block
    for (size_t i = 0; i < m->count; ++i) {
        phys_region_t *r = &m->entries[i];
        if (r->type != REGION_RAM) continue;
        uint64_t base = align_up_u64(r->start < min_addr ? min_addr : r->start, align);
        if (base + size <= r->start + r->size) {
            // ensure base..base+size does not overlap any RESERVED/MMIO entries
            phys_region_t probe = { .start = base, .size = size, .type = REGION_RESERVED };
            bool ok = true;
            for (size_t j = 0; j < m->count; ++j) {
                if (m->entries[j].type == REGION_RAM) continue;
                if (region_overlaps(&probe, &m->entries[j])) { ok = false; break; }
            }
            if (ok) { *out_base = base; return true; }
        }
    }
    return false;
}