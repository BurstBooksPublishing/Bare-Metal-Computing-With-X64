#include 

/* Save/restore structures */
struct det_mask_state {
    uint64_t rflags;   // saved RFLAGS
    uint64_t cr8;      // saved CR8 (TPR)
};

/* Read RFLAGS */
static inline uint64_t read_rflags(void) {
    uint64_t r;
    asm volatile("pushfq; pop %0" : "=r"(r) :: "memory");
    return r;
}

/* Write RFLAGS (restores IF among other flags) */
static inline void write_rflags(uint64_t r) {
    asm volatile("push %0; popfq" :: "r"(r) : "memory", "cc");
}

/* Read CR8 */
static inline uint64_t read_cr8(void) {
    uint64_t v;
    asm volatile("mov %%cr8, %0" : "=r"(v));
    return v;
}

/* Write CR8 (set task priority register) */
static inline void write_cr8(uint64_t v) {
    asm volatile("mov %0, %%cr8" :: "r"(v) : "memory");
}

/* Optional: mask a single IO-APIC redirection entry (irq -> vector) */
/* ioapic_base is a volatile mmio region mapped to the IO-APIC registers. */
static inline void ioapic_mask_redir(volatile uint32_t *ioapic_base, int index, int mask) {
    volatile uint32_t *regsel = ioapic_base;               // offset 0: reg selector
    volatile uint32_t *window = ioapic_base + 0x10/4;     // offset 0x10: data window
    uint32_t lo_index = 0x10 + index*2;
    uint32_t hi_index = lo_index + 1;
    // select low dword
    *regsel = lo_index;
    uint32_t low = *window;
    if (mask) low |= (1u << 16);
    else low &= ~(1u << 16);
    *window = low;
    // high dword unchanged here (device-specific).
    (void)hi_index;
}

/* Enter deterministic masked region */
static inline void det_mask_enter(struct det_mask_state *s, volatile uint32_t *ioapic_base, int irq_to_mask) {
    s->rflags = read_rflags();         // save flags
    s->cr8 = read_cr8();               // save CR8
    write_cr8(0xF);                    // raise TPR to highest to block APIC delivery
    asm volatile("cli" ::: "memory");  // clear IF
    if (ioapic_base)
        ioapic_mask_redir(ioapic_base, irq_to_mask, 1); // mask device IRQ at IO-APIC
    asm volatile("" ::: "memory");     // compiler barrier
}

/* Exit deterministic masked region */
static inline void det_mask_exit(struct det_mask_state *s, volatile uint32_t *ioapic_base, int irq_to_mask) {
    if (ioapic_base)
        ioapic_mask_redir(ioapic_base, irq_to_mask, 0); // unmask device IRQ
    write_rflags(s->rflags);           // restore IF and flags
    write_cr8(s->cr8);                 // restore original TPR
    asm volatile("" ::: "memory");
}