#include 
#include 
#include 

// Persistent state layout (must live in NVRAM region)
struct persist_state {
    uint64_t currentTerm;
    uint64_t votedFor;      // 0 == none
    uint64_t commitIndex;
    uint32_t crc;           // simple integrity check
} __attribute__((packed));

// Protocol frame (fixed-size)
struct frame {
    uint64_t sender_id;
    uint64_t term;
    uint64_t last_index;
    uint8_t  msg_type;
    uint8_t  pad[3];
    uint32_t crc;
} __attribute__((packed));

extern struct persist_state *PSTATE;   // mapped to NVRAM
extern uint64_t NODE_ID;
extern uint64_t LAST_LOG_INDEX;

// Hardware/driver abstractions (must be implemented per NIC)
bool tx_send_frame(const void *buf, size_t len);   // synchronous enqueue or blocking
bool rx_poll_frame(void *buf, size_t len);         // non-blocking poll, returns true if frame read
void persist_write(void *dst, const void *src, size_t len); // durable write
void persist_fence(void);                          // ensure durability (e.g., NVMe flush)

// Simple CRC function (example; replace with verified CRC)
uint32_t crc32(const void *data, size_t len);

// Attempt to start an election; returns true if this node became leader.
bool try_start_election(uint64_t election_timeout_ticks) {
    struct frame req;
    uint64_t term;
    uint64_t votes = 1; // vote for self
    // load and bump term atomically in persistent storage
    term = __atomic_load_n(&PSTATE->currentTerm, __ATOMIC_SEQ_CST) + 1;
    struct persist_state newp = *PSTATE;
    newp.currentTerm = term;
    newp.votedFor = NODE_ID;
    newp.crc = crc32(&newp, sizeof(newp)-sizeof(newp.crc));
    persist_write(PSTATE, &newp, sizeof(newp));
    persist_fence(); // ensure vote durable before broadcasting

    // build request frame
    req.sender_id = NODE_ID;
    req.term = term;
    req.last_index = LAST_LOG_INDEX;
    req.msg_type = 0; // RequestVote
    req.crc = crc32(&req, sizeof(req)-sizeof(req.crc));

    // broadcast (implementation-specific: single-send to switch, multicast, or ring)
    if (!tx_send_frame(&req, sizeof(req))) return false;

    // collect votes until timeout
    uint64_t start = rdtsc(); // TSC-based timer
    struct frame rsp;
    while (rdtsc() - start < election_timeout_ticks) {
        if (!rx_poll_frame(&rsp, sizeof(rsp))) continue;
        if (crc32(&rsp, sizeof(rsp)-sizeof(rsp.crc)) != rsp.crc) continue; // drop corrupted
        if (rsp.msg_type != 1) continue; // only process Vote messages
        if (rsp.term > term) {
            // someone has higher term: update persistent term and abort
            struct persist_state upd = *PSTATE;
            upd.currentTerm = rsp.term;
            upd.votedFor = 0;
            upd.crc = crc32(&upd, sizeof(upd)-sizeof(upd.crc));
            persist_write(PSTATE, &upd, sizeof(upd));
            persist_fence();
            return false;
        }
        if (rsp.term == term) {
            votes++;
            // quorum check: majority predicate computed atomically if needed externally
            // assume N is small and known at compile time (N_nodes).
            extern const uint32_t N_nodes;
            uint32_t Q = (N_nodes + 1) / 2 + (N_nodes % 2 != 0 ? 0 : 0); // ceil((N+1)/2)
            if (votes >= Q) {
                // become leader: write commitIndex or leader marker durably if required
                return true;
            }
        }
    }
    return false; // timeout: election failed, retry later
}