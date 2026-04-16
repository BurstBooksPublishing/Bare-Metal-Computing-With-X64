#include 
#include 

/* Replace with actual MMIO base from PCI BAR mapping. */ 
volatile uint32_t *const SENSOR_MMIO = (volatile uint32_t *)0xF0000000ULL;

/* Local APIC base from IA32_APIC_BASE MSR. Replace with discovered value. */
volatile uint32_t *const LAPIC_EOI = (volatile uint32_t *)(0xFEE00000ULL + 0xB0);

/* Power-of-two buffer size for efficient mask indexing. */
#define RING_SIZE 256
#define RING_MASK (RING_SIZE - 1)

typedef struct {
    uint32_t buf[RING_SIZE];
    _Atomic uint32_t head; /* consumer index */
    _Atomic uint32_t tail; /* producer index */
} ring_t;

ring_t sensor_ring = { .head = 0, .tail = 0 };

/* Enqueue from ISR: wait-free if ring not full. */
static inline int ring_enqueue(ring_t *r, uint32_t val) {
    uint32_t t = atomic_load_explicit(&r->tail, memory_order_relaxed);
    uint32_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    if (((t + 1) & RING_MASK) == (h & RING_MASK)) return -1; /* full */
    r->buf[t & RING_MASK] = val;                     /* MMIO value stored */
    atomic_store_explicit(&r->tail, t + 1, memory_order_release);
    return 0;
}

/* Consumer called from deterministic control loop on the same core. */
static inline int ring_dequeue(ring_t *r, uint32_t *out) {
    uint32_t h = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    if ((h & RING_MASK) == (t & RING_MASK)) return -1; /* empty */
    *out = r->buf[h & RING_MASK];
    atomic_store_explicit(&r->head, h + 1, memory_order_release);
    return 0;
}

/* GCC x86-64 interrupt attribute uses a frame argument. */
struct interrupt_frame { uint64_t rip, cs, rflags, rsp, ss; };

void sensor_isr(struct interrupt_frame *frame) __attribute__((interrupt));
void sensor_isr(struct interrupt_frame *frame)
{
    /* Read MMIO sensor register (uncached MMIO read). */
    uint32_t sample = *SENSOR_MMIO;

    /* Enqueue sample; drop if buffer full to preserve ISR latency. */
    ring_enqueue(&sensor_ring, sample);

    /* Ensure store completes before signaling EOI. */
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    /* Write End-Of-Interrupt to local APIC. */
    *LAPIC_EOI = 0;

    /* Return via compiler-generated iretq sequence for interrupt attribute. */
}