/* Minimal x86_64 C setup: build IDT entry 8 with IST index 1 and load TSS. */
#include <stdint.h>

struct idt_entry {                   /* 16-byte interrupt gate descriptor */
  uint16_t off_lo;
  uint16_t sel;
  uint8_t  ist;                      /* low 3 bits = IST, rest zero */
  uint8_t  type_attr;                /* 0x8E = present, DPL0, interrupt gate */
  uint16_t off_mid;
  uint32_t off_hi;
  uint32_t zero;
} __attribute__((packed));

struct idt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));

struct tss64 {
  uint32_t reserved0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t reserved1;
  uint64_t ist[7];
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iomap_base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;
static struct tss64 tss __attribute__((aligned(16)));
static uint8_t df_stack[4096] __attribute__((aligned(16))); /* IST1 */

/* set an IDT entry with IST index and handler pointer */
static void set_idt_entry(int vec, void (*handler)(void), uint16_t sel, uint8_t ist_index) {
  uint64_t off = (uint64_t)handler;
  idt[vec].off_lo = off & 0xFFFF;
  idt[vec].sel = sel;
  idt[vec].ist = ist_index & 0x7;
  idt[vec].type_attr = 0x8E;
  idt[vec].off_mid = (off >> 16) & 0xFFFF;
  idt[vec].off_hi = (off >> 32) & 0xFFFFFFFF;
  idt[vec].zero = 0;
}

/* external assembly handler must be PIC-safe and minimal */
extern void df_entry(void);

void init_exception_protection(void) {
  /* populate TSS IST1 with stack top for double-fault */
  tss.ist[0] = (uint64_t)(df_stack + sizeof(df_stack)); /* IST index 1 */
  /* load GDT/TSS elsewhere, then load TSS selector with ltr */
  /* install IDT double-fault vector (8) to use IST1 */
  set_idt_entry(8, df_entry, 0x08 /* code selector */, 1);
  idtp.limit = sizeof(idt)-1;
  idtp.base = (uint64_t)&idt;
  __asm__ volatile ("lidt %0"::"m"(idtp));   /* load IDT */
  __asm__ volatile ("ltr %0"::"r"((uint16_t)0x28)); /* load TSS selector */
}
/* df_entry implementation must be written in assembly to avoid ABI baggage */