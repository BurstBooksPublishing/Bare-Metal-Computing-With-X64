/* Create an IDT gate in long mode; production-ready, packed layout. */
#include <stdint.h>
struct __attribute__((packed)) idt_entry64 {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;        /* low 3 bits hold IST index */
    uint8_t  type_attr;  /* P | (DPL<<5) | type */
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};
static struct idt_entry64 idt[256];

static inline void lidt(void *ptr, uint16_t size) {
    struct { uint16_t sz; void *ptr; } __attribute__((packed)) idtr = { size-1, ptr };
    __asm__ volatile("lidt %0" : : "m"(idtr) : "memory");
}

/* set an IDT entry: vector, handler address, code selector, IST index (0..7), DPL (0..3), type (0xE/0xF) */
void idt_set_gate(int vec, void (*handler)(void), uint16_t sel, uint8_t ist, uint8_t dpl, uint8_t type) {
    uintptr_t off = (uintptr_t)handler;
    idt[vec].offset_low  = (uint16_t)off;
    idt[vec].selector    = sel;
    idt[vec].ist         = ist & 0x7;
    idt[vec].type_attr   = (uint8_t)(0x80 | ((dpl & 0x3) << 5) | (type & 0x0F)); /* P=1 */
    idt[vec].offset_mid  = (uint16_t)(off >> 16);
    idt[vec].offset_high = (uint32_t)(off >> 32);
    idt[vec].zero        = 0;
}

/* load the IDT; set up elsewhere the TSS and its IST entries and load TSS selector with ltr */
void idt_install(void) {
    lidt(idt, sizeof(idt));
}