#include 

// Read serialized start timestamp: CPUID -> RDTSC
static inline uint64_t tsc_read_start(void) {
    unsigned eax, edx;
    asm volatile(
        "cpuid\n\t"              // serialize prior instructions
        "rdtsc\n\t"              // read TSC
        : "=a"(eax), "=d"(edx)
        : "a"(0)
        : "rbx", "rcx");
    return ((uint64_t)edx << 32) | eax;
}

// Read serialized end timestamp and aux (core id if supported): RDTSCP -> CPUID
static inline uint64_t tsc_read_end(unsigned *aux) {
    unsigned eax, edx, ecx;
    asm volatile(
        "rdtscp\n\t"             // read TSC and IA32_TSC_AUX into ECX
        : "=a"(eax), "=d"(edx), "=c"(ecx)
        :
        : "rbx");
    asm volatile("cpuid" : : "a"(0) : "rbx", "rcx", "rdx"); // serialize following reads
    if (aux) *aux = ecx;
    return ((uint64_t)edx << 32) | eax;
}