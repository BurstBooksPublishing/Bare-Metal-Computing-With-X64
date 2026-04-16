#include 

/* MMIO base for NIC (platform-specific) */
#define NIC_MMIO_BASE 0xF0000000UL

volatile uint32_t *const NIC_DOORBELL = (uint32_t *)(NIC_MMIO_BASE + 0x100);

/* Fixed-size rings located in shared physical memory */
typedef struct {
    uint32_t head;         /* consumer index */
    uint32_t tail;         /* producer index */
    uint32_t size;         /* power-of-two */
    uint64_t entries[];    /* physical addresses of work descriptors */
} ring_t;

/* Preplaced at known physical addresses */
volatile ring_t *const work_ring = (ring_t *)0x80000000UL;
volatile ring_t *const resp_ring = (ring_t *)0x80001000UL;

/* Work descriptor layout (packed, placed by controller) */
typedef struct {
    uint64_t model_params_addr; /* physical address of int8 params */
    uint32_t param_count;
    uint64_t input_addr;        /* input vector */
    uint64_t output_addr;       /* where to write int32 result */
    uint32_t reserved;
} work_desc_t;

/* Memory barrier for ordering MMIO and memory accesses */
static inline void membar(void) { __asm__ volatile("mfence" ::: "memory"); }

/* Simple quantized dot-product: int8 params, int8 input, int32 accumulation */
static inline int32_t q_dot(const int8_t *params, const int8_t *input, uint32_t n) {
    int32_t acc = 0;
    for (uint32_t i = 0; i < n; ++i) {
        acc += (int32_t)params[i] * (int32_t)input[i];
    }
    return acc;
}

void worker_main(void) {
    while (1) {
        uint32_t head = work_ring->head;
        uint32_t tail = work_ring->tail;
        if (head == tail) { /* empty */
            __asm__ volatile("pause");
            continue;
        }
        uint64_t desc_phys = work_ring->entries[head & (work_ring->size - 1)];
        /* Convert phys->virt mapping assumed identity (bare-metal identity map) */
        work_desc_t *wd = (work_desc_t *)(uintptr_t)desc_phys;

        int8_t *params = (int8_t *)(uintptr_t)wd->model_params_addr;
        int8_t *input  = (int8_t *)(uintptr_t)wd->input_addr;
        int32_t  result = q_dot(params, input, wd->param_count);

        int32_t *out = (int32_t *)(uintptr_t)wd->output_addr;
        *out = result;
        membar(); /* ensure store visible before notifying */

        /* push response descriptor (reuse work descriptor phys) */
        uint32_t rtail = resp_ring->tail;
        resp_ring->entries[rtail & (resp_ring->size - 1)] = desc_phys;
        /* publish response */
        resp_ring->tail = rtail + 1;
        membar();
        /* ring NIC doorbell to transmit the response frame */
        *NIC_DOORBELL = 1; /* doorbell value defined by NIC spec */
        membar();

        /* advance work_ring head */
        work_ring->head = head + 1;
    }
}