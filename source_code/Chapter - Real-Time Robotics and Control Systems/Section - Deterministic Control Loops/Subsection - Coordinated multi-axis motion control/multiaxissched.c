#include 
#include 
#include 

#define AXES 3
#define MAX_STEPS 100000
#define GPIO_MMIO ((volatile uint32_t*)0xF0000000) // mapped step/dir register
#define TSC_FREQ 2500000000ULL // calibrated cycles per second
#define Q16_ONE (1u<<16)

static inline uint64_t rdtsc(void){ unsigned hi, lo; asm volatile("rdtsc":"=a"(lo),"=d"(hi)); return ((uint64_t)hi<<32)|lo; }

// Precomputed arrays filled during trajectory generation phase
uint32_t step_count[AXES];
uint32_t step_index[AXES];
uint64_t step_timestamps[MAX_STEPS]; // global ascending timestamps in TSC cycles
uint8_t step_axis_map[MAX_STEPS];    // which axis to pulse at each timestamp

// Busy-wait loop that toggles step pins at scheduled timestamps
void motion_execute(size_t total_events){
    uint64_t now = rdtsc();
    for(size_t e=0;e 0) { asm volatile("pause"); } // tight deterministic spin
        uint8_t axis = step_axis_map[e];
        uint32_t dirmask = (1u << (axis+8)); // example: direction pins at bits 8.. 
        uint32_t stepmask = (1u << axis);    // step pins at bits 0..2
        // Assert memory ordering; write step high then low
        GPIO_MMIO[0] = dirmask | stepmask;   // set step
        asm volatile("mfence" ::: "memory");
        GPIO_MMIO[0] = dirmask;              // clear step
        asm volatile("mfence" ::: "memory");
    }
}

// Startup: mask interrupts, calibrate TSC, compute timestamps (omitted).
int main(void){
    // ... compute step_timestamps and step_axis_map deterministically ...
    size_t total = /* computed event count */;
    assert(total < MAX_STEPS);
    motion_execute(total);
    // restore interrupts, return to safe state
    return 0;
}