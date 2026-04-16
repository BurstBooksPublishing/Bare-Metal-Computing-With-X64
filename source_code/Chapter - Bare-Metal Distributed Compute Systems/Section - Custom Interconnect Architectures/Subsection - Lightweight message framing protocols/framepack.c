#include 
#include 
/* Header is 12 bytes: version|type|len|seq|crc32 */
struct frame_hdr {
    uint8_t  ver_flags;
    uint8_t  type;
    uint16_t len;      /* payload length, little-endian */
    uint32_t seq;      /* sequence number */
    uint32_t crc;      /* CRC32 over header (crc set to 0) + payload */
} __attribute__((packed, aligned(4)));

extern uint32_t crc32_table[256]; /* precomputed table */

/* Compute CRC32 (little, standard polynomial), table-driven. */
static inline uint32_t crc32_block(const void *buf, size_t len, uint32_t init)
{
    const uint8_t *p = buf;
    uint32_t crc = ~init;
    while (len--) crc = (crc >> 8) ^ crc32_table[(crc ^ *p++) & 0xFF];
    return ~crc;
}

/* Pack a frame into provided DMA buffer. Returns total bytes written or -1. */
int pack_frame(void *dma_buf, size_t dma_cap,
               uint8_t ver_flags, uint8_t type,
               uint32_t seq, const void *payload, uint16_t payload_len)
{
    size_t total = sizeof(struct frame_hdr) + payload_len;
    if (total > dma_cap) return -1;
    struct frame_hdr *h = (struct frame_hdr *)dma_buf;
    h->ver_flags = ver_flags;
    h->type = type;
    h->len = payload_len;
    h->seq = seq;
    h->crc = 0; /* clear for CRC calc */
    /* copy payload immediately after header (zero-copy if caller provides DMA-ready ptr) */
    memcpy((uint8_t *)dma_buf + sizeof(*h), payload, payload_len);
    /* memory ordering: ensure payload and header visible before CRC write */
    __asm__ __volatile__("" ::: "memory"); /* compiler barrier */
    /* compute CRC over header + payload */
    uint32_t crc = crc32_block(dma_buf, total, 0);
    h->crc = crc;
    /* full fence before NIC descriptor kick to ensure stores reach memory */
    __asm__ __volatile__("mfence" ::: "memory");
    return (int)total;
}

/* Unpack and validate incoming DMA buffer. Returns payload pointer or NULL. */
const void *unpack_frame(void *dma_buf, size_t dma_len, uint16_t *out_len)
{
    if (dma_len < sizeof(struct frame_hdr)) return NULL;
    struct frame_hdr *h = (struct frame_hdr *)dma_buf;
    uint16_t len = h->len;
    size_t total = sizeof(*h) + len;
    if (dma_len < total) return NULL;
    uint32_t expected = h->crc;
    /* CRC over header with crc field zeroed */
    uint32_t saved = h->crc;
    h->crc = 0;
    uint32_t crc = crc32_block(dma_buf, total, 0);
    h->crc = saved;
    if (crc != expected) return NULL;
    *out_len = len;
    return (uint8_t *)dma_buf + sizeof(*h);
}