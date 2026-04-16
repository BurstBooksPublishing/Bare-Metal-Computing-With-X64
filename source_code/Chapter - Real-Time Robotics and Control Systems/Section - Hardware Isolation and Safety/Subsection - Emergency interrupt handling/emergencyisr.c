/* idt.c -- install a 64-bit interrupt gate with IST index */
#include 

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;         /* low 3 bits hold IST index */
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

extern void emergency_isr(void);

void set_idt_entry(struct idt_entry *e, void (*handler)(void),
                   uint16_t sel, uint8_t ist_index, uint8_t attr) {
    uint64_t off = (uint64_t)handler;
    e->offset_low  = off & 0xFFFF;
    e->selector    = sel;
    e->ist         = ist_index & 0x7;   // IST in low 3 bits
    e->type_attr   = attr;              // e.g., 0x8E
    e->offset_mid  = (off >> 16) & 0xFFFF;
    e->offset_high = (off >> 32) & 0xFFFFFFFF;
    e->zero        = 0;
}

/* Usage (performed during early setup):
   set_idt_entry(&idt[0xEF], emergency_isr, CODE_SEG, 1, 0x8E);
   Ensure TSS.ist[1] points at a validated kernel stack. */