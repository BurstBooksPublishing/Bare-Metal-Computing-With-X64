#include 
#include 
#include 
#include  // for _mm_mfence intrinsic

// Application-level frame header for control frames.
// Fields are compact and cache-line aligned by policy.
typedef struct {
    uint64_t seq;    // sequence number for ordering
    uint32_t tag;    // collective tag to match operations
    uint32_t size;   // payload size in bytes (0 if zero-copy reference)
    uint32_t root;   // root rank for this broadcast
    uint32_t payload_phys_low;  // low 32 bits of physical address (if zero-copy)
    uint32_t payload_phys_high; // high 32 bits of physical address
} msg_header_t;

// Transport primitives provided by earlier layer (blocking).
// Returns 0 on success, non-zero on error.
int send_frame(int peer, const msg_header_t *hdr, const void *payload, size_t len);
int recv_frame(msg_header_t *hdr_out, void *payload_buf, size_t maxlen);

// Utility to publish DMA buffer: ensure writes are globally visible.
static inline void publish_dma_buffer(void) {
    _mm_mfence(); // full memory fence before advertising DMA address
}

// Zero-copy binomial broadcast assuming p is power-of-two or not; works regardless.
int binomial_broadcast(int rank, int p, void *dma_buf, size_t size,
                       uint32_t tag, uint64_t seq, int root)
{
    msg_header_t hdr;
    int has_data = (rank == root);
    if (has_data) {
        // Ensure local writes to dma_buf are visible to NIC/peers.
        publish_dma_buffer();
    }

    // Each step exchanges the control header with a partner computed by mask.
    for (int mask = 1; mask < p; mask <<= 1) {
        int partner = rank ^ mask;
        if (partner >= p) continue;

        if (!has_data) {
            // If we don't yet have data but the partner might have it, wait to receive.
            if ((rank & mask) != 0) {
                // Expect a control frame from partner that contains DMA address.
                if (recv_frame(&hdr, NULL, 0) != 0) return -1;
                if (hdr.tag != tag || hdr.seq != seq) return -2; // mismatched collective
                // Map received physical address into local access if needed.
                // Here we assume dma_buf already refers to pinned memory that NIC wrote to.
                has_data = 1;
            } else {
                // We will receive later; skip to next mask.
                continue;
            }
        }

        if (has_data) {
            // Send header pointing to zero-copy payload (no inline payload).
            hdr.seq = seq;
            hdr.tag = tag;
            hdr.size = 0; // zero-copy indicator
            hdr.root = root;
            uintptr_t phys = (uintptr_t)dma_buf; // transport expects physical address
            hdr.payload_phys_low  = (uint32_t)(phys & 0xFFFFFFFFu);
            hdr.payload_phys_high = (uint32_t)((phys >> 32) & 0xFFFFFFFFu);
            // Flow-control: send_frame will block if peer has no credits.
            if (send_frame(partner, &hdr, NULL, 0) != 0) return -3;
        }

        // If our bit is set, we have received and should not forward further on this branch.
        if ((rank & mask) != 0) break;
    }
    return 0;
}