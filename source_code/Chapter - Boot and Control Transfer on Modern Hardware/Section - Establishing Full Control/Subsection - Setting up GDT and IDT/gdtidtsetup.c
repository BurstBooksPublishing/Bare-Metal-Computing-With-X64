#include 

/* Packed GDT entry as 64-bit value */
static uint64_t gdt[3];                /* null, code64, data64 */
struct __attribute__((packed)) gdtr { uint16_t limit; uint64_t base; } gdtr;

/* IDT entry layout (64-bit interrupt gate) */
struct __attribute__((packed)) idt_entry {
  uint16_t offset_lo;
  uint16_t selector;
  uint8_t  ist;
  uint8_t  type_attr;
  uint16_t offset_mid;
  uint32_t offset_hi;
  uint32_t zero;
};
static struct idt_entry idt[256];
struct __attribute__((packed)) idtr { uint16_t limit; uint64_t base; } idtr;

/* Make a standard 64-bit code/data descriptor (base/limit ignored for long mode) */
static uint64_t make_gdt_entry(unsigned access, unsigned flags) {
  uint64_t desc = 0;
  desc  = (uint64_t)(0xFFFF) & 0xFFFF;                // limit low (set to max)
  desc |= ((uint64_t)0x00) << 16;                     // base low
  desc |= ((uint64_t)0x00) << 32;                     // base mid + access placed below
  desc |= ((uint64_t)access) << 40;                   // access byte
  desc |= ((uint64_t)0xF) << 48;                      // limit high (0xF)
  desc |= ((uint64_t)flags) << 52;                    // flags (G,L,D,B,AVL)
  desc |= ((uint64_t)0x00) << 56;                     // base high
  return desc;
}

/* Create an IDT gate for handler */
static void set_idt_entry(int vec, void (*handler)(void), uint16_t sel, uint8_t type_attr, uint8_t ist) {
  uint64_t addr = (uint64_t)handler;
  idt[vec].offset_lo = addr & 0xFFFF;
  idt[vec].selector  = sel;
  idt[vec].ist       = ist & 0x7;
  idt[vec].type_attr = type_attr;
  idt[vec].offset_mid= (addr >> 16) & 0xFFFF;
  idt[vec].offset_hi = (addr >> 32) & 0xFFFFFFFF;
  idt[vec].zero      = 0;
}

/* Simple ISR stub; returns immediately (must not be invoked in real system). */
__attribute__((naked)) void isr_stub(void) {
  __asm__ volatile(
    "iretq\n\t"                /* placeholder: CPU will pop RIP/CS/RFLAGS when invoked */
  );
}

/* Install tables and activate them */
void install_gdt_idt(void) {
  /* Build GDT: null, code (selector 0x08), data (selector 0x10) */
  gdt[0] = 0;
  gdt[1] = make_gdt_entry(0x9A, 0x2); /* access=0x9A (P=1,S=1,type=0xA), flags: L=1,G=1 => 0x2< isr_stub, type_attr 0x8E (P=1,DPL=0,type=0xE) */
  for (int i=0;i<256;i++) idt[i].zero = 0;
  set_idt_entry(0x80, isr_stub, 0x08, 0x8E, 0);

  idtr.limit = sizeof(idt)-1;
  idtr.base  = (uint64_t)&idt;

  /* Load GDT */
  __asm__ volatile("lgdt %0" : : "m"(gdtr) : "memory");

  /* Reload CS via far return to pick up new code selector (selector = 0x08) */
  __asm__ volatile(
    "pushq $0x08\n\t"                 /* code selector */
    "lea 1f(%%rip), %%rax\n\t"
    "pushq %%rax\n\t"
    "lretq\n\t"
    "1:\n\t"
    : : : "rax", "memory"
  );

  /* Load IDT */
  __asm__ volatile("lidt %0" : : "m"(idtr) : "memory");
}