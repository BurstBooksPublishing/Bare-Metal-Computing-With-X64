#include 
#include 
#include 

/* Hardware-specific MMIO base, mapped earlier via identity mapping. */
volatile uint32_t * const MMIO_CMD   = (volatile uint32_t*)0xF0000000U;
volatile uint32_t * const MMIO_STAT  = (volatile uint32_t*)0xF0000010U;
volatile uint64_t * const MMIO_DESC  = (volatile uint64_t*)0xF0000020U;

/* Constants chosen for the target edge platform. */
enum { MAX_BATCH = 32, TENSOR_BYTES = 4096 };
_Alignas(64) static uint8_t batch_buffer[MAX_BATCH][TENSOR_BYTES];
static size_t batch_count = 0;

/* Memory barrier helpers for bare-metal x64. */
static inline void cpu_sfence(void){ asm volatile("sfence" ::: "memory"); }
static inline void cpu_mfence(void){ asm volatile("mfence" ::: "memory"); }
static inline void cpu_pause(void){ asm volatile("pause"); }

/* Submit a batch to accelerator via MMIO + prebuilt descriptor. */
static void submit_batch(size_t count)
{
    /* Ensure all tensor bytes are visible to DMA (write-back if needed). */
    cpu_sfence();

    /* Populate device descriptor: base physical address and size.
       Here we assume identity mapping, so virtual == physical. */
    MMIO_DESC[0] = (uint64_t)(uintptr_t)batch_buffer;       // base
    MMIO_DESC[1] = (uint64_t)(TENSOR_BYTES * count);        // total bytes
    cpu_mfence();

    /* Kick accelerator: write command and poll status. */
    *MMIO_CMD = (uint32_t)count;
    cpu_mfence();

    /* Poll for completion without interrupts to ensure determinism. */
    while ((*MMIO_STAT & 1u) == 0u) { cpu_pause(); }
}

/* Public API: push input; flush when batch full or on explicit request. */
void push_input(const void *input)
{
    /* copy into aligned slot */
    size_t idx = batch_count++;
    memcpy(batch_buffer[idx], input, TENSOR_BYTES);

    if (batch_count >= MAX_BATCH) {
        submit_batch(batch_count);
        batch_count = 0;
    }
}

void flush(void)
{
    if (batch_count) {
        submit_batch(batch_count);
        batch_count = 0;
    }
}