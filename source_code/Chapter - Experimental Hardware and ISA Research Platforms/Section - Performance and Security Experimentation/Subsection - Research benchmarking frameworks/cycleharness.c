#include 
#include 
#include 
#include 
#include 

// Serialize, then read TSC (RDTSCP). Returns 64-bit cycle count.
static inline uint64_t rdtscp_serialized(void) {
    unsigned int aux;
    uint64_t t;
    asm volatile("cpuid\n\t"           // serialize prior instructions
                 "rdtscp\n\t"         // read timestamp counter, ordered
                 : "=a" (((uint32_t*)&t)[0]), "=d" (((uint32_t*)&t)[1]), "=c"(aux)
                 : "a"(0)
                 : "rbx");
    return t;
}

// Example microbenchmark: increment a register N times.
static void microbench(size_t iter) {
    volatile uint64_t x = 0;
    for (size_t i = 0; i < iter; ++i) x += i;
    (void)x;
}

// Pin process to cpu_id; minimal error handling for brevity.
static void pin_cpu(int cpu_id) {
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(cpu_id, &cpus);
    sched_setaffinity(0, sizeof(cpus), &cpus);
}

int main(void) {
    const int trials = 1000;
    const size_t inner_iters = 1000;
    pin_cpu(2); // adapt to experimental core

    double mean = 0.0, m2 = 0.0;
    for (int t = 1; t <= trials; ++t) {
        uint64_t t0 = rdtscp_serialized();
        microbench(inner_iters);          // measured region
        uint64_t t1 = rdtscp_serialized();
        double sample = (double)(t1 - t0);
        double delta = sample - mean;
        mean += delta / t;
        m2 += delta * (sample - mean);    // Welford update
    }
    double variance = m2 / (trials - 1);
    printf("cycles: mean=%.2f var=%.2f\n", mean, variance);
    return 0;
}