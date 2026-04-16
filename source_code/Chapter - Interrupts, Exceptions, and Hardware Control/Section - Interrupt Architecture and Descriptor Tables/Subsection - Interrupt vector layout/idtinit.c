#include <stdint.h>

#define IDT_ENTRIES 256

/* Packed IDT entry layout for IA-32e long mode */
struct idt_entry {
    uint16_t offset_low;    /* bits 0..15 */
    uint16_t selector;      /* code segment selector */
    uint8_t  ist;           /* IST index (bits 0..2), rest zero */
    uint8_t  type_attr;     /* type and attributes */
    uint16_t offset_mid;    /* bits 16..31 */
    uint32_t offset_high;   /* bits 32..63 */
    uint32_t zero;          /* reserved */
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

/* Helper to set an IDT gate */
void idt_set_gate(uint8_t vector, void (*handler)(void),
                  uint16_t selector, uint8_t type_attr, uint8_t ist_index)
{
    uint64_t addr = (uint64_t)handler;
    idt[vector].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].ist         = (uint8_t)(ist_index & 0x7);
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vector].zero        = 0;
}

/* Install the IDT and load via lidt */
void idt_install(void)
{
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint64_t)&idt;
    /* lidt with memory operand */
    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

/* Example: set IRQ0 (timer) to vector 32, use kernel code selector 0x08 */
extern void irq0_handler(void); /* implemented in assembly */
void setup_vectors(void)
{
    uint8_t irq0_vector = 32;
    uint8_t kernel_gate = 0x8E; /* interrupt gate, present, DPL=0 */
    idt_set_gate(irq0_vector, irq0_handler, 0x08, kernel_gate, 0); /* no IST */
    /* bind NMI (vector 2) to IST index 1 for a dedicated stack */
    extern void nmi_handler(void);
    idt_set_gate(2, nmi_handler, 0x08, 0x8E, 1);
    idt_install();
}