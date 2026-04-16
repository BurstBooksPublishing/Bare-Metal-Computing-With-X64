#include <stdint.h>

#define IDT_ENTRIES 256

// Packed descriptor layout matches hardware format exactly.
typedef struct __attribute__((packed)) {
    uint16_t offset_low;    // bits 15:0
    uint16_t selector;      // code segment selector
    uint8_t  ist;           // bits 2:0 hold IST index, rest zero
    uint8_t  type_attr;     // P, DPL, zero, type
    uint16_t offset_mid;    // bits 31:16
    uint32_t offset_high;   // bits 63:32
    uint32_t zero;          // reserved, must be zero
} idt_entry_t;

static idt_entry_t idt[IDT_ENTRIES] __attribute__((aligned(16)));

struct idtr_t {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// Set one IDT entry. handler is a pointer to the function to be invoked.
static inline void set_idt_entry(unsigned int vector, void (*handler)(void),
                                 uint16_t selector, uint8_t ist,
                                 uint8_t type, uint8_t dpl, uint8_t present)
{
    uint64_t addr = (uint64_t)handler;
    idt_entry_t *e = &idt[vector];

    e->offset_low  = (uint16_t)(addr & 0xFFFF);
    e->selector    = selector;
    e->ist         = ist & 0x7; // only low 3 bits valid
    e->type_attr   = (uint8_t)((present ? 0x80 : 0x00) |
                              ((dpl & 0x3) << 5) |
                              (type & 0x0F));
    e->offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    e->offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    e->zero        = 0;
}

// Load the IDT register with the base and computed limit.
static inline void load_idt(void)
{
    struct idtr_t idtr;
    idtr.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_ENTRIES - 1);
    idtr.base  = (uint64_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idtr) : "memory");
}