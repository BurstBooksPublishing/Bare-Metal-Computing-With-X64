/* Minimal x86_64 TSS and IDT setup (GCC, flat memory, identity-mapped) */
#include <stdint.h>

#define IDT_ENTRIES 256
#define DF_VECTOR   8          /* double-fault vector */
#define DF_IST_INDEX 1         /* use IST1 for double fault */

struct __attribute__((packed)) tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};

struct __attribute__((packed)) idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;        /* low 3 bits used for IST index */
    uint8_t  type_attr;  /* P, DPL, type */
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};

static struct tss64 tss = {0};
static struct idt_entry idt[IDT_ENTRIES] = {0};
static uint8_t df_stack[4096] __attribute__((aligned(16))); /* dedicated DF stack */

/* helper: set IDT gate with IST index and interrupt handler address */
static void set_idt_gate(int vec, void (*handler)(void), uint16_t sel,
                         uint8_t type_attr, uint8_t ist_index) {
    uintptr_t addr = (uintptr_t)handler;
    idt[vec].offset_low  = (uint16_t)addr;
    idt[vec].selector    = sel;
    idt[vec].ist         = (uint8_t)(ist_index & 0x7);
    idt[vec].type_attr   = type_attr;
    idt[vec].offset_mid  = (uint16_t)(addr >> 16);
    idt[vec].offset_high = (uint32_t)(addr >> 32);
}

/* call at bootstrap before enabling interrupts */
void init_ist_and_idt(void) {
    /* Initialize IST1 to point to top of df_stack (stack grows down). */
    tss.ist1 = (uintptr_t)(df_stack + sizeof(df_stack));

    /* Create IDT gate for double-fault: interrupt gate, present, DPL=0. */
    set_idt_gate(DF_VECTOR, /* handler address */ (void*)double_fault_handler,
                 /* code selector */ 0x08, /* typical kernel CS */ 0x8E, /* P=1, type=14 */ 
                 DF_IST_INDEX);

    /* Load IDT */
    struct { uint16_t limit; uintptr_t base; } __attribute__((packed)) idtr = {
        .limit = sizeof(idt) - 1,
        .base  = (uintptr_t)idt
    };
    asm volatile ("lidt %0" : : "m"(idtr));

    /* Install TSS into GDT (GDT setup omitted here), then load TR. */
    extern uint16_t tss_selector; /* assume set by GDT init */
    asm volatile ("ltr %0" : : "r"(tss_selector));
}