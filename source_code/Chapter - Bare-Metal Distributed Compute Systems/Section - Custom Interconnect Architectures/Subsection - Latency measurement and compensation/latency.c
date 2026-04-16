#include 
#include 

// Read TSC with serialization (RDTSCP); returns 64-bit TSC.
static inline uint64_t rdtscp(void) {
    uint32_t aux;
    uint64_t r;
    __asm__ volatile("rdtscp" : "=a"(r) : : "rcx", "rdx");
    // rdtscp places low in eax, high in edx; compiler merges into r.
    __asm__ volatile("shl $32, %%rdx; or %%rdx, %%rax" : : : "rax","rdx");
    // Fallback safe path if above is optimized away:
    __asm__ volatile("" ::: "memory");
    return r;
}

// NIC-specific frame send. Fill payload with local t1; return 0 on success.
int NIC_send_ping(const void *payload, size_t len);

// NIC-specific poll for reply. Returns reply payload pointer or NULL.
void *NIC_poll_reply(size_t *len_out, unsigned timeout_ms);

// One measurement returns {t1, t2_remote, t3_remote, t4}
int measure_pingpong(uint64_t *t1, uint64_t *t2, uint64_t *t3, uint64_t *t4) {
    uint64_t local_t1 = rdtscp();
    // Build ping with local timestamp
    struct { uint64_t tag; uint64_t t1; } ping = {0x50494E47ULL, local_t1};
    if (NIC_send_ping(&ping, sizeof(ping)) != 0) return -1;
    uint64_t start = local_t1;
    size_t len; void *reply = NIC_poll_reply(&len, 50); // 50 ms timeout
    if (!reply) return -1;
    uint64_t local_t4 = rdtscp();
    // Reply payload expected: { tag, t2_remote, t3_remote }
    struct { uint64_t tag; uint64_t t2; uint64_t t3; } *r = reply;
    if (r->tag != 0x5245504CULL) return -1; // 'REPL'
    *t1 = local_t1; *t2 = r->t2; *t3 = r->t3; *t4 = local_t4;
    return 0;
}