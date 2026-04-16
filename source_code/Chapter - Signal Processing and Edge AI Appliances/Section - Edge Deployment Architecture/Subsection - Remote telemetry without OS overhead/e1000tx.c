/* Minimal, single-producer TX function. Assumes ring and MMIO inited. */
#include 
#include 
#define RING_SZ 1024
#define TX_CMD_EOP 0x01
#define TX_CMD_RS  0x08
#define TX_STATUS_DD 0x1

struct tx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed, aligned(16)));

volatile struct tx_desc *tx_ring = /* DMA mapped ring base */;
volatile uint8_t *tx_buffers = /* DMA mapped buffer region */;
volatile uint32_t *mmio_TDT = /* device transmit descriptor tail */;
static uint32_t tx_index = 0;

/* Read invariant TSC reliably */
static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    asm volatile("" ::: "memory");
    return ((uint64_t)hi << 32) | lo;
}

/* Ensure device sees writes before doorbell */
static inline void sfence_wb(void) { asm volatile("sfence" ::: "memory"); }

/* Send one telemetry payload (raw UDP payload pointer) */
int telemetry_tx(const void *payload, uint16_t payload_len) {
    if (payload_len > 1500) return -1; /* enforce MTU */
    uint32_t idx = tx_index & (RING_SZ - 1);
    volatile struct tx_desc *d = &tx_ring[idx];

    /* Build Ethernet/IP/UDP headers into tx_buffers[idx * 2048] region */
    uint8_t *buf = (uint8_t*)(tx_buffers + idx * 2048);
    uint16_t pkt_len = 0;

    /* Timestamp and append payload (timestamp first, then payload) */
    uint64_t tsc = read_tsc();
    memcpy(buf, &tsc, sizeof(tsc)); pkt_len += sizeof(tsc);
    memcpy(buf + pkt_len, payload, payload_len); pkt_len += payload_len;

    /* Zero descriptor status before handing to device */
    d->status = 0;
    /* Set buffer physical address previously prepared in init */
    /* d->buffer_addr already contains physical address for buf */

    /* Device visible length and cmd flags */
    d->length = pkt_len;
    sfence_wb(); /* ensure descriptor fields and buffer are visible */
    d->cmd = TX_CMD_EOP | TX_CMD_RS;

    /* Doorbell: advance tail */
    tx_index++;
    *mmio_TDT = tx_index & 0xFFFF;
    sfence_wb();

    /* Poll for completion with short busy-wait; deterministic */
    for (int i = 0; i < 100000; ++i) {
        if (d->status & TX_STATUS_DD) return 0;
        /* optional small pause to yield pipeline */
        asm volatile("pause" ::: "memory");
    }
    return -2; /* timeout */
}