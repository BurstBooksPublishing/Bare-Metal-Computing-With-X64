/* Compile: gcc -O2 -march=native microbench.c -o microbench */
#include 
#include 
#include 
#include 
#include 

static void pin_to_cpu(int cpu) {
    cpu_set_t mask; CPU_ZERO(&mask); CPU_SET(cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask)) perror("sched_setaffinity");
}

/* dependent chain: measures latency L (cycles). */
uint64_t measure_latency(unsigned long iterations) {
    uint64_t t0 = __rdtsc();
    uint64_t r = 1;
    for (unsigned long i = 0; i < iterations; ++i) {
        asm volatile("add %1, %0\n\t" : "+r"(r) : "r"(i) : ); /* dependent add */
    }
    uint64_t t1 = __rdtsc();
    (void)r; /* prevent optimization removing loop */
    return t1 - t0;
}

/* independent streams: measure reciprocal throughput u (cycles/inst).
   We unroll across 4 registers to create independent streams. */
uint64_t measure_throughput(unsigned long iterations) {
    uint64_t t0 = __rdtsc();
    register uint64_t r0 = 1, r1 = 2, r2 = 3, r3 = 4;
    for (unsigned long i = 0; i < iterations; ++i) {
        asm volatile(
            "add %2, %0\n\t"
            "add %2, %1\n\t"
            "add %2, %3\n\t"
            "add %2, %4\n\t"
            : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
            : "r"(i)
            : );
    }
    uint64_t t1 = __rdtsc();
    (void)r0;(void)r1;(void)r2;(void)r3;
    return t1 - t0;
}

int main(void) {
    pin_to_cpu(0);
    const unsigned long N = 20000000UL;
    /* warm up and then measure */
    volatile uint64_t w = measure_latency(1000);
    (void)w;
    uint64_t cycles_lat = measure_latency(N);
    uint64_t cycles_thr = measure_throughput(N / 4); /* loop did 4 adds per iter */

    double cycles_per_inst_lat = (double)cycles_lat / (double)N;
    double cycles_per_inst_thr = (double)cycles_thr / (double)(N);
    printf("Latency-mode cycles/inst ~ %.4f\n", cycles_per_inst_lat);
    printf("Throughput-mode cycles/inst ~ %.4f\n", cycles_per_inst_thr);
    return 0;
}