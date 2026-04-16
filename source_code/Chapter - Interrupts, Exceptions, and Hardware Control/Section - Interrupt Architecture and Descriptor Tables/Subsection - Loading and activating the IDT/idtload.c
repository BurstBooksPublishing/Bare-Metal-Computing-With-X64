#include 

/* Packed 64-bit IDT gate layout matched to hardware. */
struct idt_entry {
    uint16_t offset_low;    // bits 0..15 of handler address
    uint16_t selector;      // code segment selector
    uint8_t  ist;           // IST index (bits 0..2), rest zero
    uint8_t  type_attr;     // P, DPL, type (0x8E for interrupt gate)
    uint16_t offset_mid;    // bits 16..31
    uint32_t offset_high;   // bits 32..63
    uint32_t zero;          // reserved
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Aligned IDT large enough for 256 vectors. Keep in .bss or dedicated region. */
static struct idt_entry idt[256] __attribute__((aligned(16)));
static struct idtr idtr_hdr;

/* External ISR stubs implemented in assembly; addresses taken by linker. */
extern void isr_timer(void);    // e.g., vector 0x20 (IRQ0)
extern void isr_syscall(void);  // e.g., vector 0x80 (software interrupt)

/* Fill a single IDT gate. ist_index: 0..7, type_attr: typically 0x8E for interrupt gate. */
static inline void set_idt_entry(int vec, void (*handler)(void),
                                 uint16_t selector, uint8_t ist_index,
                                 uint8_t type_attr)
{
    uintptr_t addr = (uintptr_t)handler;
    idt[vec].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vec].selector    = selector;
    idt[vec].ist         = (uint8_t)(ist_index & 0x7);
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vec].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vec].zero        = 0;
}

/* Build the IDT entries and atomically load IDTR. */
void install_idt(void)
{
    /* Example selectors and attributes for long mode kernel code segment. */
    const uint16_t KERNEL_CS = 0x08;      /* typical GDT selector for kernel code */
    const uint8_t  INTERRUPT_GATE = 0x8E; /* present, ring 0, type 0xE */

    /* Zero the IDT to start in deterministic state. */
    for (int i = 0; i < 256; ++i) {
        idt[i] = (struct idt_entry){0};
    }

    /* Populate selected vectors with handlers and IST usage. */
    set_idt_entry(0x20, isr_timer,   KERNEL_CS, /*ist_index=*/1, INTERRUPT_GATE);
    set_idt_entry(0x80, isr_syscall, KERNEL_CS, /*ist_index=*/0, INTERRUPT_GATE);

    /* Prepare IDTR and load it atomically. */
    idtr_hdr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr_hdr.base  = (uint64_t)(uintptr_t)&idt[0];

    asm volatile (
        "cli\n\t"                /* disable interrupts while switching tables */
        "lidt %0\n\t"            /* load IDTR from memory operand */
        : /* no outputs */
        : "m"(idtr_hdr)
        : "memory"
    );

    /* Caller may re-enable interrupts with 'sti' when ready. */
}