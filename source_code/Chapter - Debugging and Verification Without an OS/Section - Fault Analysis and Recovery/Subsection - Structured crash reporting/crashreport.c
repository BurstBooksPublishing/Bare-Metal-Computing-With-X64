#include 
#include 

#define CRASH_MAGIC 0x43524153u  /* 'CRAS' */
#define CRASH_SCHEMA 1
#define COM1_PORT 0x3F8
#define CRASH_AREA_BASE ((volatile uint8_t*)0x1000000)  /* reserved physical region */
#define CRASH_AREA_SIZE (64 * 1024)
#define RECORD_SIZE 512
#define RECORDS_MAX (CRASH_AREA_SIZE / RECORD_SIZE)

/* packed crash record */
struct crash_record {
    uint32_t magic;
    uint16_t schema;
    uint16_t reserved;
    uint64_t seq;
    uint64_t tsc;
    uint64_t rip;
    uint64_t rsp;
    uint64_t cr3;
    uint64_t cr2;
    uint8_t stack_sample[64];
    uint64_t regs[16]; /* rax..r15 snapshot */
    uint8_t padding[RECORD_SIZE - 4-2-2-8*7-64-16*8-4];
    uint32_t crc;
} __attribute__((packed));

static inline uint64_t rdtsc(void) {
    unsigned a, d;
    __asm__ volatile ("rdtsc" : "=a"(a), "=d"(d));
    return ((uint64_t)d << 32) | a;
}

static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(v));
    return v;
}

/* small but correct bitwise CRC32 (polynomial 0xEDB88320) */
static uint32_t crc32(const void *buf, size_t len) {
    const uint8_t *p = buf;
    uint32_t crc = ~0u;
    for (size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
    return ~crc;
}

/* outb for COM1 */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* write ASCII hex to serial (very small, safe in handlers) */
static void serial_hex64(uint64_t v) {
    const char hex[] = "0123456789ABCDEF";
    for (int i = 15; i >= 0; --i) {
        outb(COM1_PORT, hex[(v >> (i*4)) & 0xF]);
    }
    outb(COM1_PORT, '\n');
}

/* main reporter: caller provides a snapshot of regs and a small stack sample pointer */
void crash_report_emit(uint64_t regs[16], const void *stack_ptr) {
    static volatile uint8_t *area = CRASH_AREA_BASE;
    static uint64_t seq = 0;
    size_t slot = (seq % RECORDS_MAX);
    struct crash_record *rec = (struct crash_record*)(area + slot * RECORD_SIZE);

    struct crash_record tmp = {0};
    tmp.magic = CRASH_MAGIC;
    tmp.schema = CRASH_SCHEMA;
    tmp.seq = ++seq;
    tmp.tsc = rdtsc();
    tmp.rip = regs[16-1]; /* assume caller placed RIP in regs[15] convention */
    tmp.rsp = (uint64_t)stack_ptr;
    tmp.cr3 = read_cr3();
    tmp.cr2 = read_cr2();
    for (int i = 0; i < 16; ++i) tmp.regs[i] = regs[i];
    /* copy small stack sample safely */
    const uint8_t *sp = (const uint8_t*)stack_ptr;
    for (size_t i = 0; i < sizeof(tmp.stack_sample); ++i) tmp.stack_sample[i] = sp[i];

    /* compute CRC excluding trailing crc field */
    tmp.crc = crc32(&tmp, sizeof(tmp) - sizeof(tmp.crc));

    /* ordered store sequence: payload, memory-order barrier, seq (atomicity by seq), barrier, crc */
    __asm__ volatile("mfence" ::: "memory");
    for (size_t i = 0; i < sizeof(tmp); i += 8) {
        *(volatile uint64_t*)((uint8_t*)rec + i) = *(uint64_t*)((uint8_t*)&tmp + i);
    }
    __asm__ volatile("mfence" ::: "memory");

    /* also emit brief serial summary: seq, tsc, rip */
    serial_hex64(tmp.seq);
    serial_hex64(tmp.tsc);
    serial_hex64(tmp.rip);
}