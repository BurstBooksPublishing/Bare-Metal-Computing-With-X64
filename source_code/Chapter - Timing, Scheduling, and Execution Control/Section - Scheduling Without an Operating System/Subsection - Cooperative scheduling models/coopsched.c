/* context.S (GAS, Intel syntax) */
    .intel_syntax noprefix
    .globl context_switch
    .type context_switch, @function
/* args: rdi = old_tcb, rsi = new_tcb */
context_switch:
    /* save callee-saved regs into old_tcb */
    mov qword ptr [rdi + 0x00], rbx
    mov qword ptr [rdi + 0x08], rbp
    mov qword ptr [rdi + 0x10], r12
    mov qword ptr [rdi + 0x18], r13
    mov qword ptr [rdi + 0x20], r14
    mov qword ptr [rdi + 0x28], r15
    mov qword ptr [rdi + 0x30], rsp
    /* save return address (caller pushed it on stack) */
    mov rax, qword ptr [rsp]
    mov qword ptr [rdi + 0x38], rax

    /* restore callee-saved regs from new_tcb */
    mov rbx, qword ptr [rsi + 0x00]
    mov rbp, qword ptr [rsi + 0x08]
    mov r12, qword ptr [rsi + 0x10]
    mov r13, qword ptr [rsi + 0x18]
    mov r14, qword ptr [rsi + 0x20]
    mov r15, qword ptr [rsi + 0x28]
    mov rsp, qword ptr [rsi + 0x30]
    mov rax, qword ptr [rsi + 0x38]
    jmp rax

/* scheduler.c */
#include 
/* Task Control Block layout must match assembly offsets above */
typedef struct tcb { uint64_t rbx, rbp, r12, r13, r14, r15, rsp, rip; } tcb_t;
extern void context_switch(tcb_t *old, tcb_t *new);

/* Initialize TCB to run fn with given stack_top. stack_top must be 16-byte aligned. */
static inline void tcb_init(tcb_t *t, void (*fn)(void), void *stack_top) {
    t->rbx = t->rbp = t->r12 = t->r13 = t->r14 = t->r15 = 0;
    t->rsp = (uint64_t)stack_top;
    /* Push initial fake return address so that context_switch jmp to rip works */
    t->rsp -= 8;
    *((uint64_t*)t->rsp) = 0;            /* when task returns, jmp 0 -> fault or handled */
    t->rip = (uint64_t)fn;
}

/* Example usage: two tasks yield cooperatively */
tcb_t taskA, taskB, *current;
void yield_to(tcb_t *next) { context_switch(current, next); current = next; }

/* Tasks would invoke yield_to(&other) at appropriate points. */
/* Real system must provide stack memory, watchdog, and overrun detection. */