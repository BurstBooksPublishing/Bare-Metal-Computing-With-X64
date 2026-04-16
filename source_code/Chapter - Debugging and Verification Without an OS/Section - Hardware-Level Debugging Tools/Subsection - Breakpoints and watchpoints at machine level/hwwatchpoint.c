#include 

// Helper macros for DR7 fields; safe for uint64_t operations.
#define DR7_LOCAL_ENABLE(i)  (1ULL << (2*(i)))
#define DR7_GLOBAL_ENABLE(i) (1ULL << (2*(i) + 1))
#define DR7_RW(i,val)        ((uint64_t)(val) << (16 + 4*(i))) // RW at 16+4*i
#define DR7_LEN(i,val)       ((uint64_t)(val) << (18 + 4*(i))) // LEN at 18+4*i

// RW encodings: 0=exec,1=write,3=read/write
// LEN encodings: 0=1B,1=2B,3=4B,2=8B

static inline void write_dr(unsigned reg, uint64_t value) {
    // reg must be 0..7; only valid in ring-0
    switch (reg) {
    case 0: __asm__ volatile("mov %0, %%dr0"  : : "r"(value)); break;
    case 1: __asm__ volatile("mov %0, %%dr1"  : : "r"(value)); break;
    case 2: __asm__ volatile("mov %0, %%dr2"  : : "r"(value)); break;
    case 3: __asm__ volatile("mov %0, %%dr3"  : : "r"(value)); break;
    case 6: __asm__ volatile("mov %0, %%dr6"  : : "r"(value)); break;
    case 7: __asm__ volatile("mov %0, %%dr7"  : : "r"(value)); break;
    default: __asm__ volatile("ud2"); // unsupported in this helper
    }
}

static inline uint64_t read_dr(unsigned reg) {
    uint64_t val;
    switch (reg) {
    case 0: __asm__ volatile("mov %%dr0, %0"  : "=r"(val)); break;
    case 1: __asm__ volatile("mov %%dr1, %0"  : "=r"(val)); break;
    case 2: __asm__ volatile("mov %%dr2, %0"  : "=r"(val)); break;
    case 3: __asm__ volatile("mov %%dr3, %0"  : "=r"(val)); break;
    case 6: __asm__ volatile("mov %%dr6, %0"  : "=r"(val)); break;
    case 7: __asm__ volatile("mov %%dr7, %0"  : "=r"(val)); break;
    default: __asm__ volatile("ud2"); // unsupported
    }
    return val;
}

// Install a local write watchpoint on addr (4 bytes).
void install_write_watchpoint(void *addr) {
    // Ensure privileged environment. Set DR0 to linear address to watch.
    write_dr(0, (uint64_t)addr);

    // Clear DR6 sticky status.
    write_dr(6, 0);

    // Build DR7: enable local for index 0; RW=1 (write); LEN=3 (4 bytes).
    uint64_t dr7 = 0;
    dr7 |= DR7_LOCAL_ENABLE(0);
    dr7 |= DR7_RW(0, 1);   // write
    dr7 |= DR7_LEN(0, 3);  // 4-byte length

    write_dr(7, dr7);
    // After this, writes to *addr trigger debug exception (#DB).
}