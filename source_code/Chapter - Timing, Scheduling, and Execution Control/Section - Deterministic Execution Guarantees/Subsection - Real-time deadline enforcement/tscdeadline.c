#include <stdint.h>

#define IA32_TSC_DEADLINE 0x6E0ULL

// Read TSC via RDTSCP (serializing read)
static inline uint64_t read_tsc(void) {
    uint32_t hi, lo;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return ((uint64_t)hi << 32) | lo;
}

// Write to MSR (WRMSR) - bare-metal privilege required
static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" :: "c"(msr), "a"(low), "d"(high));
}

// Arm a one-shot deadline at absolute TSC value 'deadline'
void arm_tsc_deadline(uint64_t deadline) {
    write_msr((uint32_t)IA32_TSC_DEADLINE, deadline);
}

// Example: schedule a task with WCET cycles 'wcet_cycles'
// epsilon_cycles must account for worst-case interrupt latency
void schedule_task_with_deadline(uint64_t wcet_cycles, uint64_t epsilon_cycles) {
    uint64_t now = read_tsc();
    uint64_t deadline = now + wcet_cycles + epsilon_cycles;
    arm_tsc_deadline(deadline);
    // start task's critical section immediately after arming
}