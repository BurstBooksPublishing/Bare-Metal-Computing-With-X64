#include 
#include 
#include 

// Wire header (packed, network byte order)
struct rtp_hdr {
    uint8_t  ver_flags;   // version:2 | flags:6
    uint8_t  pad;
    uint32_t seq;
    uint32_t ack;
    uint16_t len;
    uint16_t reserved;
    uint32_t crc32;
} __attribute__((packed));

// Transmit entry stored in local ring (power-of-two size)
struct tx_entry {
    atomic_bool acked;
    uint32_t seq;
    uint64_t sent_ts_ms;
    uint16_t len;
    uint8_t *payload; // pointer into DMA-safe buffer
};

// Globals (tuned at init)
#define TX_RING_SZ 1024
static struct tx_entry tx_ring[TX_RING_SZ];
static atomic_uint_fast32_t tx_head; // next slot to fill (mod TX_RING_SZ)
static atomic_uint_fast32_t tx_tail; // oldest unacked

static uint32_t next_seq = 1;
static const uint32_t timeout_ms = 200; // retransmit timeout

// Application calls this to send a payload (non-blocking if ring has space)
int send_payload(uint8_t *dma_buf, uint16_t len)
{
    uint32_t head = atomic_load_explicit(&tx_head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&tx_tail, memory_order_acquire);
    if (((head + 1) & (TX_RING_SZ - 1)) == (tail & (TX_RING_SZ - 1)))
        return -1; // ring full

    uint32_t idx = head & (TX_RING_SZ - 1);
    struct tx_entry *e = &tx_ring[idx];

    // prepare header in DMA buffer
    struct rtp_hdr hdr = {0};
    hdr.ver_flags = (1<<6); // version in high bits
    hdr.seq = htonl(next_seq);
    hdr.ack = htonl(0);
    hdr.len = htons(len);
    // compute CRC if desired, omitted for brevity

    memcpy(dma_buf, &hdr, sizeof(hdr));
    // payload already in dma_buf + sizeof(hdr)
    e->payload = dma_buf;
    e->len = len;
    e->seq = next_seq;
    atomic_store_explicit(&e->acked, false, memory_order_release);

    // post to NIC TX (descriptor points to dma_buf, length = hdr+len)
    nic_post_tx(dma_buf, sizeof(hdr) + len);
    e->sent_ts_ms = now_ms();

    next_seq++;
    atomic_store_explicit(&tx_head, head + 1, memory_order_release);
    return 0;
}

// Called from RX path when a frame arrives
void on_frame_received(uint8_t *buf, uint16_t total_len)
{
    if (total_len < sizeof(struct rtp_hdr)) return;
    struct rtp_hdr hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    uint32_t seq = ntohl(hdr.seq);
    uint32_t ack = ntohl(hdr.ack);
    uint16_t payload_len = ntohs(hdr.len);

    // process ACKs (cumulative)
    if (ack) {
        // mark entries with seq <= ack as acked
        uint32_t tail = atomic_load_explicit(&tx_tail, memory_order_relaxed);
        uint32_t head = atomic_load_explicit(&tx_head, memory_order_acquire);
        for (uint32_t s = tail; s != head; s++) {
            uint32_t idx = s & (TX_RING_SZ - 1);
            if (tx_ring[idx].seq <= ack)
                atomic_store_explicit(&tx_ring[idx].acked, true, memory_order_release);
            else break;
        }
        // advance tail past acked entries
        while (atomic_load_explicit(&tx_tail, memory_order_relaxed) != atomic_load_explicit(&tx_head, memory_order_acquire)) {
            uint32_t t = atomic_load_explicit(&tx_tail, memory_order_relaxed);
            uint32_t ti = t & (TX_RING_SZ - 1);
            if (atomic_load_explicit(&tx_ring[ti].acked, memory_order_acquire))
                atomic_store_explicit(&tx_tail, t + 1, memory_order_release);
            else break;
        }
    }

    // process data frame: maintain receive window (omitted buffer code for brevity)
    if (payload_len && !(hdr.ver_flags & 0x20)) {
        // deliver or buffer based on seq; then send cumulative ACK
        uint32_t cum_ack = deliver_and_get_cumulative_ack(seq, buf + sizeof(hdr), payload_len);
        // send small ACK frame
        uint8_t ack_buf[64];
        struct rtp_hdr ack_hdr = {0};
        ack_hdr.ver_flags = (1<<6) | 0x40; // ACK flag
        ack_hdr.seq = htonl(0);
        ack_hdr.ack = htonl(cum_ack);
        ack_hdr.len = htons(0);
        memcpy(ack_buf, &ack_hdr, sizeof(ack_hdr));
        nic_post_tx(ack_buf, sizeof(ack_hdr));
    }
}

// Periodic timer: scans for retransmit
void retransmit_tick(void)
{
    uint64_t now = now_ms();
    uint32_t tail = atomic_load_explicit(&tx_tail, memory_order_acquire);
    uint32_t head = atomic_load_explicit(&tx_head, memory_order_acquire);
    for (uint32_t s = tail; s != head; s++) {
        uint32_t idx = s & (TX_RING_SZ - 1);
        struct tx_entry *e = &tx_ring[idx];
        if (atomic_load_explicit(&e->acked, memory_order_acquire)) continue;
        if (now - e->sent_ts_ms >= timeout_ms) {
            // retransmit
            nic_post_tx(e->payload, sizeof(struct rtp_hdr) + e->len);
            e->sent_ts_ms = now;
        }
    }
}