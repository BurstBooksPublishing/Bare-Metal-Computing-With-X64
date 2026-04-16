#include 
#include 

/* Device MMIO: example ADC sample register and LAPIC EOI address (platform-specific). */
volatile uint32_t * const ADC_SAMPLE_REG = (uint32_t*)0xFEC00000; /* example MMIO */
volatile uint32_t * const LAPIC_EOI      = (uint32_t*)0xFEE000B0; /* local APIC EOI */

/* Configure at compile time: power-of-two buffer size for fast masking. */
#define BUF_LOG2 12
#define BUF_SIZE (1U << BUF_LOG2)
#define BUF_MASK (BUF_SIZE - 1U)
static int16_t ring[BUF_SIZE] __attribute__((aligned(64)));
static _Atomic uint32_t head = 0; /* producer (ISR) index */
static _Atomic uint32_t tail = 0; /* consumer index */
static _Atomic uint64_t drop_count = 0;

/* Push one sample into ring; ISR-only caller. */
static inline void ring_push_isr(int16_t s)
{
    uint32_t h = atomic_load_explicit(&head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&tail, memory_order_acquire);
    uint32_t next = (h + 1) & BUF_MASK;
    if (next == (t & BUF_MASK)) {
        /* Buffer full: drop sample and count it. Keep ISR short. */
        atomic_fetch_add_explicit(&drop_count, 1, memory_order_relaxed);
        return;
    }
    ring[h & BUF_MASK] = s; /* single-writer, no atomic store needed for data */
    /* Publish the new head with release semantics so consumer sees data. */
    atomic_store_explicit(&head, next, memory_order_release);
}

/* Consumer: pop one sample; returns 0 if empty, 1 if sample returned in *out. */
static inline int ring_pop(int16_t *out)
{
    uint32_t t = atomic_load_explicit(&tail, memory_order_relaxed);
    uint32_t h = atomic_load_explicit(&head, memory_order_acquire);
    if ((t & BUF_MASK) == (h & BUF_MASK)) return 0; /* empty */
    *out = ring[t & BUF_MASK];
    atomic_store_explicit(&tail, (t + 1) & BUF_MASK, memory_order_release);
    return 1;
}

/* Platform: write LAPIC EOI (must be device-specific mapping). */
static inline void lapic_eoi(void)
{
    /* Ensure prior MMIO reads/writes complete before EOI. */
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    *LAPIC_EOI = 0;
}

/* ISR: minimal, attribute interrupt makes compiler emit proper prologue/epilogue.
   'frame' parameter type platform-dependent; unused here. */
void __attribute__((interrupt)) irq_sampler_handler(void *frame)
{
    /* Read one sample from ADC MMIO and push to ring. Keep cache footprint minimal. */
    uint32_t raw = *ADC_SAMPLE_REG;           /* device-specific sample format */
    int16_t sample = (int16_t)(raw & 0xFFFF); /* example conversion */
    ring_push_isr(sample);
    lapic_eoi();                              /* inform APIC we're done */
    /* return via compiler-generated IRETQ sequence for interrupt attribute */
}