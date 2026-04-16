#include 
#include 

/* Ring element header placed at buffer[offset]. */
struct ring_hdr {
    uint64_t seq;      /* monotonic sequence number */
    uint32_t len;      /* payload length in bytes */
    uint32_t crc32;    /* payload checksum */
};

/* Memory-mapped ring buffer descriptor (set by platform init). */
volatile uint8_t * const ring_base = (volatile uint8_t *)0xFEC00000; /* MMIO example */
const size_t RING_CAP = 1 << 20; /* 1 MiB ring */

static inline uint32_t crc32_hw(const void *data, size_t len); /* platform CRC */

/* Append an event atomically. Returns sequence number on success. */
uint64_t append_event(const void *payload, uint32_t len) {
    static uint64_t next_seq = 1; /* persistent across reboots if desired */
    uint64_t seq = __atomic_fetch_add(&next_seq, 1, __ATOMIC_RELAXED);

    /* compute slot offset (power-of-two alignment recommended) */
    size_t slot_sz = sizeof(struct ring_hdr) + ((len + 7) & ~7);
    size_t idx = (seq * slot_sz) & (RING_CAP - 1);
    volatile uint8_t *slot = ring_base + idx;
    struct ring_hdr hdr = { seq, len, 0 };

    /* write payload after header in local buffer to avoid torn writes */
    uint8_t *tmp = (uint8_t *)payload; /* assume caller-owned buffer */

    /* compute CRC before publication */
    hdr.crc32 = crc32_hw(payload, len);

    /* store header with seq=0 as not-ready sentinel */
    struct ring_hdr sentinel = { 0, 0, 0 };
    __atomic_store_n((struct ring_hdr *)slot, sentinel, __ATOMIC_RELAXED);

    /* write payload bytes (non-temporal stores optional) */
    for (uint32_t i = 0; i < len; ++i) slot[sizeof(hdr) + i] = tmp[i];

    /* full memory fence to order stores before making header visible */
    __atomic_thread_fence(__ATOMIC_RELEASE);

    /* publish header with full metadata and sequence */
    __atomic_store_n((struct ring_hdr *)slot, hdr, __ATOMIC_RELEASE);

    return seq;
}