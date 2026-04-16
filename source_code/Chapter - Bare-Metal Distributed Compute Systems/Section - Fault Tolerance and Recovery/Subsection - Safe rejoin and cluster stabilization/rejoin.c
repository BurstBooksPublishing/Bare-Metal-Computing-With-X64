#include 
#include 

// External platform primitives (implemented per-target).
extern void flush_caches_privileged(void);         // executes wbinvd; ring-0
extern void memory_fence(void);                   // mfence
extern bool send_frame(const void *buf, size_t len); // raw frame send
extern int recv_frame(void *buf, size_t len, int timeout_ms); // raw recv
extern bool persistent_read(void *dst, size_t len); // e.g., NVRAM
extern bool persistent_write(const void *src, size_t len);
extern void apply_checkpoint_bytes(const void *data, size_t len);

// Protocol frame structures.
typedef struct { uint64_t epoch; uint8_t digest[32]; } join_req_t;
typedef struct { uint64_t known_epoch; uint8_t accept; uint8_t digest[32]; } join_resp_t;

#define NODES_QUORUM 3
#define FRAME_MAX 1500
static uint8_t frame_buf[FRAME_MAX];

bool publish_epoch_atomic(uint64_t expected, uint64_t new_epoch) {
    // Atomic publish using platform atomic CAS on persistent storage.
    // Implementation must map to LOCK CMPXCHG on persistent cell.
    return __atomic_compare_exchange_n(&((volatile uint64_t*)0)[0],
                                       &expected, new_epoch, false,
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

bool safe_rejoin(void) {
    uint64_t local_epoch;
    uint8_t local_digest[32];
    if (!persistent_read(&local_epoch, sizeof(local_epoch))) return false;
    if (!persistent_read(local_digest, sizeof(local_digest))) return false;

    flush_caches_privileged();            // ensure no stale dirty data
    memory_fence();

    join_req_t req = { .epoch = local_epoch + 1 };
    memcpy(req.digest, local_digest, sizeof(req.digest));
    send_frame(&req, sizeof(req));

    int responses = 0;
    uint64_t highest_seen = local_epoch;
    join_resp_t resp;
    int start = 0;
    while (responses < NODES_QUORUM && start < 5000) {
        if (recv_frame(&frame_buf, FRAME_MAX, 100)) {
            memcpy(&resp, frame_buf, sizeof(resp));
            responses++;
            if (resp.known_epoch > highest_seen) highest_seen = resp.known_epoch;
        }
        start += 100;
    }
    if (responses < NODES_QUORUM) return false; // cannot get quorum

    if (highest_seen > local_epoch) {
        // fetch checkpoint for highest_seen
        // send explicit fetch request (omitted frame type details for brevity)
        // receive, validate digest, then apply
        // Assume fetch yields 'checkpoint_buf' with size 'ck_size'
        extern bool fetch_checkpoint(uint64_t epoch, void **buf, size_t *ck_size);
        void *ck_buf; size_t ck_size;
        if (!fetch_checkpoint(highest_seen, &ck_buf, &ck_size)) return false;
        // validate digest (omitted SHA256 code for brevity)
        apply_checkpoint_bytes(ck_buf, ck_size);
        local_epoch = highest_seen;
        // persist new epoch
        if (!persistent_write(&local_epoch, sizeof(local_epoch))) return false;
    }

    // publish a new epoch to mark rejoin; must be atomic with persistent region
    uint64_t expected = local_epoch;
    uint64_t new_epoch = local_epoch + 1;
    if (!publish_epoch_atomic(expected, new_epoch)) return false;

    // final fence and resume
    memory_fence();
    return true;
}