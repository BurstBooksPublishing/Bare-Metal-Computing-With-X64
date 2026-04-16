#include 
#include 

volatile uint32_t *const ADC_CTRL = (uint32_t *)0xF0000000; // control register base
volatile uint32_t *const ADC_STATUS = (uint32_t *)0xF0000010; // status register
volatile uint32_t *const ADC_DMA_ADDR = (uint32_t *)0xF0000020; // DMA descriptor base
volatile uint32_t *const ADC_START = (uint32_t *)0xF0000030; // start conversion

#define SAMPLE_BUF_LEN 1024
typedef struct { uint64_t tsc; uint32_t sample; uint32_t flags; } sample_t;
alignas(64) static sample_t sample_buf[SAMPLE_BUF_LEN]; // cache-line aligned

static inline uint64_t rdtscp_serialized(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    __asm__ volatile("mfence" ::: "memory"); // order loads/stores wrt timestamp
    return ((uint64_t)hi << 32) | lo;
}

/* Initialize DMA descriptor pointing to sample_buf; hardware reads into buffer. */
void adc_dma_init(void) {
    *ADC_DMA_ADDR = (uint32_t)((uintptr_t)sample_buf); // low 32 bits for example
    // set control: circular mode, buffer length
    *ADC_CTRL = (1<<0) | (SAMPLE_BUF_LEN & 0xFFFF); // enable DMA + length
    __asm__ volatile("sfence" ::: "memory"); // ensure writes reach device
}

/* Start continuous acquisition and poll status for buffer wrap. */
void adc_acquire_loop(void) {
    adc_dma_init();
    *ADC_START = 1; // start conversions
    size_t read_index = 0;
    while (1) {
        uint32_t status = *ADC_STATUS; // device updates index atomically
        if (status != read_index) {
            // sample_buf[read_index] already filled by DMA; timestamp capture is post-DMA
            uint64_t t = rdtscp_serialized(); // tight timestamp after DMA completion visibility
            sample_buf[read_index].tsc = t; // attach timestamp (if device didn't)
            // process or enqueue for Kalman filter here (non-blocking)
            read_index = (read_index + 1) & (SAMPLE_BUF_LEN - 1);
        }
        // optional: low-power wait or yield to other real-time tasks
    }
}