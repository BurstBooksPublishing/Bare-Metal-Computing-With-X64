#include <stdint.h>
#include <string.h>

#define TX_RING_SIZE 256
#define CACHELINE 64

/* MMIO register offsets (device-specific; adapt to your NIC) */
#define REG_TX_DESC_ADDR_LO 0x3800
#define REG_TX_DESC_LEN     0x3808
#define REG_TX_HEAD         0x3810
#define REG_TX_TAIL         0x3818
#define REG_TX_CTRL         0x3820

/* Generic descriptor */
struct tx_desc {
    uint64_t buffer_addr;  /* physical address for DMA */
    uint32_t length;
    uint32_t flags;
} __attribute__((packed, aligned(16)));

static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(4096)));
static void *tx_buffers[TX_RING_SIZE]; /* pointers to payload buffers */
static volatile uint32_t *mmio;        /* mapped NIC MMIO base (word-addressable) */
static uint16_t tx_tail = 0;

/* Platform-specific: return physical address for DMA: here we assume identity map. */
static inline uintptr_t phys_addr(void *v) { return (uintptr_t)v; }

/* Flush cache lines covering [p, p+len) using clflushopt if available. */
static inline void clflush_range(void *p, size_t len) {
    uintptr_t a = (uintptr_t)p & ~(CACHELINE - 1);
    uintptr_t end = (uintptr_t)p + len;
    for (; a < end; a += CACHELINE) {
        __asm__ volatile("clflushopt (%0)" :: "r"((void*)a) : "memory");
    }
    __asm__ volatile("sfence" ::: "memory"); /* ensure flush completes before proceeding */
}

/* MMIO helpers (32-bit registers) */
static inline void mmio_write32(uint32_t off, uint32_t val) {
    mmio[off / 4] = val;
    __asm__ volatile("" ::: "memory"); /* compiler barrier */
}

/* Initialize TX ring registers (call after PCI BAR mapping). */
void tx_ring_init(void) {
    uintptr_t phys = phys_addr(tx_ring);
    mmio_write32(REG_TX_DESC_ADDR_LO, (uint32_t)phys);
    mmio_write32(REG_TX_DESC_LEN, TX_RING_SIZE * sizeof(struct tx_desc));
    mmio_write32(REG_TX_HEAD, 0);
    mmio_write32(REG_TX_TAIL, 0);
    /* optional: set TX_CTRL to enable transmitter */
}

/* Enqueue a single frame (blocking if ring full) */
int tx_send_frame(void *buf, uint32_t len) {
    uint16_t tail = tx_tail;
    uint16_t next = (tail + 1) % TX_RING_SIZE;
    /* Simple fullness check: if next == head, ring is full. Read head from device. */
    uint16_t head = (uint16_t)mmio[REG_TX_HEAD / 4];
    if (next == head) return -1; /* ring full */

    /* Copy frame into preallocated DMA buffer; here caller provides DMA-safe buffer. */
    void *dma_buf = tx_buffers[tail];
    for (uint32_t i = 0; i < len; ++i) ((uint8_t*)dma_buf)[i] = ((uint8_t*)buf)[i];

    /* Flush payload from CPU cache to memory so NIC DMA reads correct data. */
    clflush_range(dma_buf, len);

    /* Populate descriptor: ensure stores reach memory before tail update. */
    tx_ring[tail].buffer_addr = (uint64_t)phys_addr(dma_buf);
    tx_ring[tail].length = len;
    tx_ring[tail].flags = 0x1; /* device-specific flags: EOP, RS, etc. */

    /* Ensure descriptor writes are visible before notifying device */
    __asm__ volatile("sfence" ::: "memory");

    /* Update tail register to hand descriptor to NIC */
    mmio_write32(REG_TX_TAIL, next);
    tx_tail = next;
    return 0;
}