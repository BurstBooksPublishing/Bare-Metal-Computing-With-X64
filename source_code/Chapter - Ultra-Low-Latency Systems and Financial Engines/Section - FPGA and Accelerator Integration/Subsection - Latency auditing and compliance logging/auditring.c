#include <stdint.h>
#include <x86intrin.h>
#include <stdatomic.h>

#define RING_SIZE_LOG2 12
#define RING_SIZE (1u<<RING_SIZE_LOG2)
#define RECORD_SIZE 64
#define MMIO_BASE 0x40000000

typedef struct {
    uint64_t seq;
    uint64_t fpga_ts;
    uint64_t cpu_ts;
    uint32_t event_id;
    uint32_t crc;
} audit_record_t;

_Static_assert(sizeof(audit_record_t) == RECORD_SIZE, "Record size mismatch");

static audit_record_t *ring = (audit_record_t *)MMIO_BASE;
static _Atomic(uint32_t) prod;

void audit_write(uint32_t event_id, uint64_t fpga_ts) {
    uint32_t idx = atomic_fetch_add(&prod, 1) & (RING_SIZE - 1);
    audit_record_t *r = &ring[idx];
    r->seq = idx;
    r->fpga_ts = fpga_ts;
    r->cpu_ts = __rdtscp(NULL);
    r->event_id = event_id;
    r->crc = 0; /* compute CRC32 over record */
}