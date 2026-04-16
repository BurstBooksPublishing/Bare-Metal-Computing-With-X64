#include 
#include 
#include 
#include 

static inline uint64_t rdtsc_start(void) {
    unsigned int eax, edx;
    asm volatile("cpuid\n\t" "rdtsc\n\t"
                 : "=a"(eax), "=d"(edx) : "a"(0) : "rbx","rcx");
    return ((uint64_t)edx << 32) | eax;
}
static inline uint64_t rdtsc_end(void) {
    unsigned int eax, edx;
    asm volatile("rdtscp\n\t" "mov %%eax, %0\n\t" "mov %%edx, %1\n\t" "cpuid\n\t"
                 : "=r"(eax), "=r"(edx) :: "rax","rbx","rcx","rdx");
    return ((uint64_t)edx << 32) | eax;
}

int main(void) {
    const size_t N = 10'000'000;
    uint8_t *data = aligned_alloc(64, N);
    if (!data) return 1;
    // predictable pattern: first half 0, second half 1
    for (size_t i = 0; i < N; ++i) data[i] = (i < N/2);
    volatile uint64_t sink = 0;
    uint64_t t0 = rdtsc_start();
    for (size_t i = 0; i < N; ++i) {
        if (data[i]) sink += i; // branch highly predictable
    }
    uint64_t t1 = rdtsc_end();
    printf("predictable: %.2f cycles/iter\n", (double)(t1 - t0) / N);

    // randomize pattern to defeat predictor
    srand((unsigned)time(NULL));
    for (size_t i = 0; i < N; ++i) data[i] = rand() & 1;
    t0 = rdtsc_start();
    for (size_t i = 0; i < N; ++i) {
        if (data[i]) sink += i; // branch unpredictable
    }
    t1 = rdtsc_end();
    printf("unpredictable: %.2f cycles/iter\n", (double)(t1 - t0) / N);
    // keep sink to avoid dead-store elimination
    printf("sink=%" PRIu64 "\n", sink);
    free(data);
    return 0;
}