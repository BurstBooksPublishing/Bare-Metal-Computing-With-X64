/* Minimal serial output (polling UART0, COM1 at 0x3F8). */
static inline void uart_putc(char c) {
    while ((*(volatile unsigned char*)0x3F8 + 5) & 0x20 == 0) {} /* wait THR empty */
    *(volatile unsigned char*)0x3F8 = (unsigned char)c;
}
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

/* Hex dump of 64-bit value. */
static void uart_hex64(unsigned long long v) {
    const char *hex = "0123456789ABCDEF";
    for (int i = 15; i >= 0; --i) {
        uart_putc(hex[(v >> (i*4)) & 0xF]);
    }
    uart_putc('\n');
}

/* Trap frame layout produced by the assembly wrapper below. */
struct trap_frame {
    unsigned long long r15, r14, r13, r12, r11, r10, r9, r8;
    unsigned long long rsi, rdi, rbp, rdx, rcx, rbx, rax;
    unsigned long long vec_no;       /* pushed by wrapper for dispatch */
    unsigned long long rip, cs, rflags, rsp, ss; /* CPU-pushed on fault */
};

/* C handler called by assembly wrapper. Print registers and control regs. */
void dump_registers(struct trap_frame *tf) {
    uart_puts("RAX: "); uart_hex64(tf->rax);
    uart_puts("RBX: "); uart_hex64(tf->rbx);
    uart_puts("RCX: "); uart_hex64(tf->rcx);
    uart_puts("RDX: "); uart_hex64(tf->rdx);
    uart_puts("RSI: "); uart_hex64(tf->rsi);
    uart_puts("RDI: "); uart_hex64(tf->rdi);
    uart_puts("RBP: "); uart_hex64(tf->rbp);
    uart_puts("RSP: "); uart_hex64(tf->rsp);
    uart_puts("RIP: "); uart_hex64(tf->rip);
    uart_puts("RFLAGS: "); uart_hex64(tf->rflags);

    unsigned long long cr2, cr3;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    uart_puts("CR2 (fault addr): "); uart_hex64(cr2);
    uart_puts("CR3 (PML4): "); uart_hex64(cr3);

    /* Optionally capture FPU/SSE state with FXSAVE to a 512-byte aligned buffer. */
}

/* Naked ISR wrapper: saves volatile registers, pushes vector number, calls C handler. */
__attribute__((naked)) void isr_common(void) {
    __asm__ __volatile__(
        /* Save caller-saved and callee-saved registers we care about. */
        "push   rax\n\t"
        "push   rbx\n\t"
        "push   rcx\n\t"
        "push   rdx\n\t"
        "push   rbp\n\t"
        "push   rdi\n\t"
        "push   rsi\n\t"
        "push   r8\n\t"
        "push   r9\n\t"
        "push   r10\n\t"
        "push   r11\n\t"
        "push   r12\n\t"
        "push   r13\n\t"
        "push   r14\n\t"
        "push   r15\n\t"
        /* Push vector number (assume placed in al by IDT descriptor stub). */
        "push   rax\n\t"
        "mov    rdi, rsp        /* argument: pointer to struct trap_frame */\n\t"
        "call   dump_registers\n\t"
        /* Tear down stack in reverse order and iretq to return. */
        "add    rsp, 8*16+8    /* pop saved regs + vec_no */\n\t"
        "iretq\n\t"
    );
}