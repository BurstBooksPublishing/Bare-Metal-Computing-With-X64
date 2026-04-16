#include 
#include 

/* Read serialized TSC via RDTSCP; returns timestamp in RAX:RDX. */
static inline uint64_t read_tsc_rdtscp(void) {
    uint32_t aux;
    uint64_t t;
    __asm__ volatile("rdtscp" : "=a"(t), "=d"(aux) : : "rcx");
    return t;
}

/* Simple cache/TLB warm: touch memory sequentially. */
void warm_pages(void *buf, size_t bytes, size_t stride) {
    volatile uint8_t *p = (volatile uint8_t*)buf;
    for (size_t i = 0; i < bytes; i += stride) p[i];
}

/* Measure N iterations of a callback; cli/sti to mask interrupts. */
void measure_jitter(void (*callback)(void), uint64_t *out, size_t N) {
    uint64_t t0, t1;
    /* mask interrupts (clears IF). */
    __asm__ volatile("cli");
    /* warm critical code and data beforehand (caller must supply buffer). */
    /* barrier to ensure warm completes before timing. */
    __asm__ volatile("lfence");
    for (size_t i = 0; i < N; ++i) {
        t0 = read_tsc_rdtscp();
        callback();
        t1 = read_tsc_rdtscp();
        out[i] = t1 - t0;
    }
    /* restore interrupt enable. */
    __asm__ volatile("sti");
}