#include 

/* IDT entry format for 64-bit interrupt gate */
struct idt_entry {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;         /* bits 0-2: IST, bits 3-7: zero */
  uint8_t type_attr;   /* P, DPL, type */
  uint16_t offset_mid;
  uint32_t offset_high;
  uint32_t zero;
} __attribute__((packed));

struct idtr { uint16_t limit; uint64_t base; } __attribute__((packed));
extern void syscall_entry(void); /* assembly entry point */

static struct idt_entry idt[256];
static struct idtr idtr;

/* set an IDT interrupt gate with DPL=3 to allow user invocation */
void set_idt_gate(int vec, void *handler) {
    uint64_t off = (uint64_t)handler;
    idt[vec].offset_low  = off & 0xffff;
    idt[vec].selector    = 0x08;               /* kernel code selector */
    idt[vec].ist         = 0;                  /* or chosen IST index */
    idt[vec].type_attr   = 0xEE;               /* P=1, DPL=3, type=0xE (interrupt gate) */
    idt[vec].offset_mid  = (off >> 16) & 0xffff;
    idt[vec].offset_high = (off >> 32) & 0xffffffff;
    idt[vec].zero        = 0;
}

/* Compact token-validation entry called from assembly stub.
   regs points to saved registers laid down by the stub. */
void syscall_dispatch(uint64_t *regs) {
    uint64_t token_ptr = regs[0]; /* convention: user places token in RDI before INT */
    if (token_ptr == 0) return;   /* deny null token */
    /* validate token range: bound to user address space (example) */
    if (token_ptr >= 0x0000800000000000ULL) return; /* simple user-space bound check */
    /* Copy and cryptographically verify token; omitted for brevity */
    /* if validated, map to allowed actions and perform minimal privileged operation */
}

/* install IDT and enable vector 0x80 */
void install_syscall_vector(void) {
    idtr.limit = sizeof(idt)-1;
    idtr.base  = (uint64_t)idt;
    set_idt_gate(0x80, syscall_entry);
    asm volatile ("lidt %0" :: "m"(idtr));     /* load IDT */
    /* set up TSS.RSP0 elsewhere and load TR (ltr) */
}