#include 

/* APIC offsets */
#define APIC_EOI   0x000000B0
#define APIC_ICRLO 0x00000300
#define APIC_ICRHI 0x00000310

/* Exported from linker or set from MSR IA32_APIC_BASE; here assume 0xFEE00000 */
volatile uint32_t *lapic = (volatile uint32_t *)0xFEE00000;

/* Shared buffer for timestamps recorded by ISR */
volatile uint64_t isr_timestamps[1024];
volatile uint32_t isr_index = 0;

/* Helpers: write local APIC register (32-bit) */
static inline void lapic_write(uint32_t off, uint32_t v) {
    lapic[off/4] = v;
    (void)lapic[APIC_EOI/4]; /* readback to ensure write posted */
}

/* Issue an IPI to self using shorthand (set destination in ICRHI zero, ICRLO shorthand) */
static inline void send_self_ipi(uint32_t vector) {
    /* vector (bits 0..7), type=Fixed (0), shorthand=Self (0x40000) */
    uint32_t icr = vector | 0x00004000; /* Fixed delivery, no flags, shorthand self */
    lapic_write(APIC_ICRLO, icr);
}

/* Read serializing TSC in userspace-friendly inline asm */
static inline uint64_t rdtsc_serialized(void) {
    unsigned a, d;
    asm volatile("lfence\n\t" "rdtsc" : "=a"(a), "=d"(d) :: "memory");
    return ((uint64_t)d << 32) | a;
}

/* Assembly ISR defined below (push regs, rdtscp, store, EOI, iretq) */
extern void isr_vector_stub(void);

/* Install IDT entry for vector 0xF0 omitted (assume already installed to isr_vector_stub). */
/* Measurement loop: record tsc_before, send IPI, wait for isr_index increment, compute delta */
void measure_loop(void) {
    for (int i = 0; i < 1000; ++i) {
        uint64_t t0 = rdtsc_serialized();
        uint32_t start = isr_index;
        send_self_ipi(0xF0); /* vector chosen to match IDT entry */
        /* wait for ISR to record */
        while (isr_index == start) asm volatile("pause");
        uint64_t t1 = isr_timestamps[start];
        volatile uint64_t delta = t1 - t0;
        /* store or print delta (platform-specific I/O omitted). */
        (void)delta;
    }
}

/* Linker must place this in an IRQ-capable code section; keep minimal prologue/epilogue. */
__asm__ (
".globl isr_vector_stub\n\t"
"isr_vector_stub:\n\t"
"    pushq %rax\n\t"
"    pushq %rcx\n\t"
"    pushq %rdx\n\t"
"    pushq %rsi\n\t"
"    pushq %rdi\n\t"
"    pushq %rbx\n\t"
"    pushq %rbp\n\t"
"    pushq %r8\n\t"
"    pushq %r9\n\t"
"    pushq %r10\n\t"
"    pushq %r11\n\t"
"    /* get TSC via RDTSCP */\n\t"
"    rdtscp\n\t"
"    shlq $32, %rdx\n\t"
"    orq %rax, %rdx\n\t"
"    /* store timestamp into buffer */\n\t"
"    movq isr_index(%rip), %rax\n\t"
"    leaq isr_timestamps(%rip), %rcx\n\t"
"    movq %rdx, (%rcx,%rax,8)\n\t"
"    addq $1, isr_index(%rip)\n\t"
"    /* EOI */\n\t"
"    movl $0, 0xB0(%rip) /* not portable: replace with lapic base access if needed */\n\t"
"    /* restore regs */\n\t"
"    popq %r11\n\t"
"    popq %r10\n\t"
"    popq %r9\n\t"
"    popq %r8\n\t"
"    popq %rbp\n\t"
"    popq %rbx\n\t"
"    popq %rdi\n\t"
"    popq %rsi\n\t"
"    popq %rdx\n\t"
"    popq %rcx\n\t"
"    popq %rax\n\t"
"    iretq\n\t"
);