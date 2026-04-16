#include 
/* Read invariant TSC (RDTSCP) with serialization. */
static inline uint64_t rdtsc_u64(void) {
    unsigned int aux;
    uint64_t lo, hi;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux) ::);
    return (hi << 32) | lo;
}

/* Busy-wait until absolute TSC deadline (production: prefer LAPIC deadline). */
void wait_until_tsc(uint64_t deadline_cycles) {
    while (1) {
        uint64_t now = rdtsc_u64();
        if (now >= deadline_cycles) break;
        __asm__ volatile("pause"); /* reduce pipeline contention */
    }
}

/* Example: schedule next wake given period_cycles. */
void control_loop_tick(uint64_t period_cycles) {
    static uint64_t next_deadline = 0;
    uint64_t now = rdtsc_u64();
    if (next_deadline == 0) next_deadline = now + period_cycles;
    else next_deadline += period_cycles; /* avoid drift accumulation */

    /* Execute control computation here (bounded to C cycles). */

    wait_until_tsc(next_deadline); /* block until next period */
}