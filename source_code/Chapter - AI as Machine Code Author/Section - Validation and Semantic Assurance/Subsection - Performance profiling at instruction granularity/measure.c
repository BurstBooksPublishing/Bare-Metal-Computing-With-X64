#include 
#include 

/* Read time-stamp counter after serializing using CPUID then RDTSC */
static inline uint64_t rdtsc_start(void) {
    uint32_t hi, lo;
    asm volatile("cpuid\n\t"            /* serialize */
                 "rdtsc\n\t"
                 : "=d"(hi), "=a"(lo) : : "rbx", "rcx");
    return ((uint64_t)hi << 32) | lo;
}

/* Read time-stamp counter with RDTSCP which serializes on later CPUs */
static inline uint64_t rdtsc_end(void) {
    uint32_t hi, lo;
    asm volatile("rdtscp\n\t"          /* serializing rdtsc */
                 : "=d"(hi), "=a"(lo) : : "rcx");
    asm volatile("cpuid\n\t" : : : "rax","rbx","rcx","rdx"); /* serialize again */
    return ((uint64_t)hi << 32) | lo;
}

/* Measure average cycles of fn executed reps times; caller runs in ring 0. */
uint64_t measure_cycles(void (*fn)(void), size_t reps) {
    volatile size_t i;
    uint64_t t0, t1;

    /* Warmup to fill caches and branch predictors */
    for (i = 0; i < 16; ++i) fn();

    /* Optionally disable interrupts to avoid preemption (bare-metal) */
    asm volatile("cli" ::: "memory");

    t0 = rdtsc_start();
    for (i = 0; i < reps; ++i) fn();
    t1 = rdtsc_end();

    asm volatile("sti" ::: "memory"); /* re-enable interrupts */

    return (t1 - t0) / reps;
}