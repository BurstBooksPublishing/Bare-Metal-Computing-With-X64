#include 
#include 

// Physical MMIO base addresses for three independent controllers.
#define CH_A_ADDR 0xFEC00000UL
#define CH_B_ADDR 0xFEC01000UL
#define CH_C_ADDR 0xFEC02000UL

// Offsets for command register inside each controller region.
#define CMD_OFF    0x00

static inline void mfence_barrier(void) {
    asm volatile("mfence" ::: "memory"); // full ordering for MMIO
}

static inline uint32_t read_mmio32(uintptr_t addr) {
    volatile uint32_t *ptr = (volatile uint32_t *)addr;
    uint32_t v = *ptr; // volatile read
    mfence_barrier();
    return v;
}

// Application-defined safe stop action.
void safe_stop(void); // implemented elsewhere

// Voter: majority vote among three 32-bit command words.
uint32_t tmr_vote_command(void) {
    uint32_t a = read_mmio32(CH_A_ADDR + CMD_OFF);
    uint32_t b = read_mmio32(CH_B_ADDR + CMD_OFF);
    uint32_t c = read_mmio32(CH_C_ADDR + CMD_OFF);

    if (a == b || a == c) return a; // a agrees with at least one peer
    if (b == c) return b;           // b and c agree
    // No majority: indicate invalid by protocol-defined value.
    return 0xFFFFFFFFU;
}

// Control loop snippet: deterministic, called at fixed rate.
void control_loop_iteration(void) {
    static int disagreement_count = 0;
    const int N_fault = 3; // threshold before safe stop
    uint32_t cmd = tmr_vote_command();
    if (cmd == 0xFFFFFFFFU) {
        if (++disagreement_count >= N_fault) safe_stop(); // persistent fault
    } else {
        disagreement_count = 0;
        // Send voted command to actuator via isolated channel (not shown).
        apply_actuator_command(cmd); // deterministic, low-latency routine
    }
}