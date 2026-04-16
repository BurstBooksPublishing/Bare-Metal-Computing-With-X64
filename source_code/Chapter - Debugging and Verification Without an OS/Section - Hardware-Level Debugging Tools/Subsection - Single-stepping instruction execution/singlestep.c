/* single_step.c - minimal single-step support for bare-metal x86-64 */
#include 

/* Assembly interrupt handler: write 'S' to COM1 and return via IRETQ. */
asm volatile(
".intel_syntax noprefix\n"
".globl db_single_step\n"
"db_single_step:\n"
"    push rax\n"                 /* preserve used register */
"    mov dx, 0x3F8\n"           /* COM1 port */
"    mov al, 'S'\n"             /* marker for single-step event */
"    out dx, al\n"              /* write character to UART */
"    pop rax\n"
"    iretq\n"
".att_syntax prefix\n"
);

/* Enable TF (bit 8) in RFLAGS. */
static inline void enable_single_step(void) {
    unsigned long tmp;
    asm volatile(
        "pushfq\n\t"
        "pop %0\n\t"
        "or $0x100, %0\n\t"
        "push %0\n\t"
        "popfq\n\t"
        : "=&r"(tmp)
        :
        : "memory");
}

/* Disable TF in RFLAGS. */
static inline void disable_single_step(void) {
    unsigned long tmp;
    asm volatile(
        "pushfq\n\t"
        "pop %0\n\t"
        "and $~0x100, %0\n\t"
        "push %0\n\t"
        "popfq\n\t"
        : "=&r"(tmp)
        :
        : "memory");
}