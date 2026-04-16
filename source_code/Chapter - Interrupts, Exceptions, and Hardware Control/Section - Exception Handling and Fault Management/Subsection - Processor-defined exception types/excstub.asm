.intel_syntax noprefix
.section .text
.global exception_init
.extern common_exception_handler         # C function: void common_exception_handler(uint64_t vec, uint64_t err, struct regs*)
# Macro: define exception stub for vector n, push synthetic error if needed.
.macro EXC_STUB vec, has_error
.global .exc_\vec
.exc_\vec:
    /* save general purpose registers (caller-saved preserved too for diagnostics) */
    push    rbp
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    /* push vector number and error code (if no error, push 0) */
    mov     rdi, \vec            /* first argument: vector number */
    \ifnum \has_error == 0
        xor     rsi, rsi         /* second arg: synthetic error code = 0 */
    \else
        /* error code already pushed by CPU lands at (RSP) ; move to RSI */
        mov     rsi, [rsp]       /* copy error code into rsi */
        add     rsp, 8           /* remove cpu-pushed error code from stack */
    \endif
    /* pass pointer to saved regs as third argument (current rsp) */
    lea     rdx, [rsp]          /* third arg: pointer to saved register frame */
    call    common_exception_handler
    /* restore registers (common handler must prepare for resume or halt) */
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
    pop     rbp
    /* iretq or similar: handler will have prepared appropriate stack/context */
    iretq
.endm

/* Example instantiations: vector 14 (page fault) pushes an error code; vector 3 (breakpoint) does not. */
EXC_STUB 14, 1
EXC_STUB 3, 0