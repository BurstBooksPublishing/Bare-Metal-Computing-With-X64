#include 
#include 
#include 
#include  // for _mm_sfence()

#define SLOT_COUNT 2
#define MODEL_MAX_BYTES (1<<20) // 1 MiB per slot

typedef struct {
    uint64_t version;
    uint32_t crc32;
    uint32_t size;
} __attribute__((packed, aligned(8))) model_hdr_t;

static volatile uint8_t model_storage[SLOT_COUNT][MODEL_MAX_BYTES];
static volatile model_hdr_t model_hdrs[SLOT_COUNT] __attribute__((aligned(8)));

// CRC32 (polynomial 0xEDB88320). Table computed at compile-time or init.
static const uint32_t crc32_table[256] = { /* 256-entry table omitted for brevity */ };

static uint32_t crc32_compute(const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t crc = ~0u;
    for (size_t i = 0; i < len; ++i)
        crc = (crc >> 8) ^ crc32_table[(crc ^ p[i]) & 0xFF];
    return ~crc;
}

// Atomically publish new model into inactive slot and commit.
int publish_model(const void *data, size_t size) {
    if (size > MODEL_MAX_BYTES - sizeof(model_hdr_t)) return -1;
    // select inactive slot (the one with lower version)
    uint64_t v0 = model_hdrs[0].version;
    uint64_t v1 = model_hdrs[1].version;
    int slot = (v0 <= v1) ? 0 : 1;

    // copy payload to slot (after header area)
    uint8_t *dst = (uint8_t *)&model_storage[slot][sizeof(model_hdr_t)];
    memcpy(dst, data, size);

    // compute crc on payload
    uint32_t crc = crc32_compute(dst, size);

    // prepare header: increment version deterministically
    uint64_t new_version = (v0 <= v1 ? v1 : v0) + 1;

    // write metadata in order: size, crc, then version with fence
    // size and crc are non-volatile, but order must be visible before version.
    model_hdr_t tmp = { .version = 0, .crc32 = crc, .size = (uint32_t)size };
    memcpy((void *)&model_hdrs[slot], &tmp, sizeof(tmp));
    _mm_sfence(); // ensure size+crc are visible before version store

    // publish version atomically (8-byte aligned write)
    model_hdrs[slot].version = new_version;
    _mm_sfence(); // ensure version write completes

    return 0;
}

// Reader: select highest-version valid slot
int select_valid_model(uint8_t **out_ptr, uint32_t *out_size) {
    uint64_t v0 = model_hdrs[0].version;
    uint64_t v1 = model_hdrs[1].version;
    int slot = (v0 >= v1) ? 0 : 1;
    model_hdr_t hdr = model_hdrs[slot]; // snapshot
    if (hdr.version == 0) return -1;
    uint8_t *payload = (uint8_t *)&model_storage[slot][sizeof(model_hdr_t)];
    if (crc32_compute(payload, hdr.size) != hdr.crc32) return -1;
    *out_ptr = payload;
    *out_size = hdr.size;
    return 0;
}

// Minimal watchdog poke via MMIO address (platform-specific)
static inline void feed_watchdog(volatile uint32_t *mmio_watchdog) {
    *mmio_watchdog = 0xABCD1234; // magic poke
    _mm_sfence();
}