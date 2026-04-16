#include <stdint.h>
#include <stddef.h>

/* Platform-specific: CPU frequency in Hz (calibrated at boot). */
extern uint64_t cpu_hz;

/* Minimal task descriptor. wcet_cycles must be measured beforehand. */
typedef struct {
    void (*entry)(void);
    uint64_t period_cycles;
    uint64_t wcet_cycles;
    uint64_t next_deadline;
} task_t;

/* Example tasks */
void servo_task(void);
void safety_task(void);

task_t tasks[] = {
    { servo_task,  1000000ULL, 300000ULL, 0 },
    { safety_task, 10000000ULL, 1000000ULL, 0 }
};
const size_t task_count = sizeof(tasks)/sizeof(tasks[0]);

static inline uint64_t rdtsc64(void) {
    unsigned hi, lo;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void cli(void){ __asm__ volatile("cli" ::: "memory"); }
static inline void sti(void){ __asm__ volatile("sti" ::: "memory"); }
static inline void cpu_pause(void){ __asm__ volatile("pause"); }

volatile uint32_t * const WATCHDOG = (uint32_t*)0xFEC00000;

void schedule_loop(void) {
    uint64_t now = rdtsc64();
    for (size_t i = 0; i < task_count; ++i)
        tasks[i].next_deadline = now + tasks[i].period_cycles;

    while (1) {
        now = rdtsc64();
        uint64_t next_deadline = tasks[0].next_deadline;

        for (size_t i = 0; i < task_count; ++i) {
            if ((int64_t)(now - tasks[i].next_deadline) >= 0) {
                uint64_t start = rdtsc64();
                cli();
                tasks[i].entry();
                sti();
                if (rdtsc64() - start > tasks[i].wcet_cycles)
                    *WATCHDOG = 0xDEAD0001;
                tasks[i].next_deadline += tasks[i].period_cycles;
            }
            if (tasks[i].next_deadline < next_deadline)
                next_deadline = tasks[i].next_deadline;
        }

        while ((int64_t)(rdtsc64() - next_deadline) < 0)
            cpu_pause();
    }
}