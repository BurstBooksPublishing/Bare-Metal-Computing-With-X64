; Intel syntax, NASM-style. 'current_task' points to struct task_state*, 'next_task' set by scheduler.
global isr_timer_entry
extern scheduler         ; scheduler(current_task) -> returns next_task in rax
section .text
isr_timer_entry:
    cli                         ; disable interrupts early (deterministic)
    push rbp                    ; create frame if needed
    push rax rcx rdx rbx        ; save volatile caller regs
    push rsi rdi r8 r9 r10 r11  ; save more volatiles
    mov rdi, [rel current_task] ; rdi = pointer to current task_state
    ; store general-purpose registers to task image
    mov [rdi + 0*8], rax
    mov [rdi + 1*8], rbx
    mov [rdi + 2*8], rcx
    mov [rdi + 3*8], rdx
    mov [rdi + 4*8], rsi
    mov [rdi + 5*8], rdi
    mov [rdi + 6*8], rbp
    mov [rdi + 7*8], rsp            ; saved stack pointer
    mov [rdi + 8*8], r8
    mov [rdi + 9*8], r9
    mov [rdi + 10*8], r10
    mov [rdi + 11*8], r11
    mov [rdi + 12*8], r12
    mov [rdi + 13*8], r13
    mov [rdi + 14*8], r14
    mov [rdi + 15*8], r15
    ; save RIP and RFLAGS from stack (hardware-pushed RIP/CS/RFLAGS are at top of stack)
    mov rax, [rsp + 8*9]        ; depends on pushes above (adjust correctly for frame)
    mov [rdi + 16*8], rax       ; save RIP
    pushfq
    pop rax
    mov [rdi + 16*8 + 8], rax   ; save RFLAGS
    ; save CR3 (page table base)
    mov rax, cr3
    mov [rdi + 16*8 + 16], rax
    ; save FPU/SIMD state (ensure destination 16-byte aligned)
    lea rdx, [rdi + 16*8 + 24]  ; address of fxsave buffer
    fxsave [rdx]                ; 512-byte image
    ; call scheduler(current_task) -> returns next_task in rax
    mov rdi, [rel current_task]
    call scheduler
    test rax, rax
    jz .no_switch               ; if same task, restore and return
    ; update current_task pointer and restore next task
    mov [rel current_task], rax
    mov rdi, rax                ; rdi = next task_state
    ; restore FPU/SIMD
    lea rdx, [rdi + 16*8 + 24]
    fxrstor [rdx]
    ; restore CR3 (switch address space)
    mov rax, [rdi + 16*8 + 16]
    mov cr3, rax
    ; restore saved registers
    mov rax, [rdi + 0*8]
    mov rbx, [rdi + 1*8]
    mov rcx, [rdi + 2*8]
    mov rdx, [rdi + 3*8]
    mov rsi, [rdi + 4*8]
    mov rdi, [rdi + 5*8]
    mov rbp, [rdi + 6*8]
    mov rsp, [rdi + 7*8]
    mov r8,  [rdi + 8*8]
    mov r9,  [rdi + 9*8]
    mov r10, [rdi + 10*8]
    mov r11, [rdi + 11*8]
    mov r12, [rdi + 12*8]
    mov r13, [rdi + 13*8]
    mov r14, [rdi + 14*8]
    mov r15, [rdi + 15*8]
    ; restore RIP and RFLAGS by IRETQ (pop returns frame pushed by CPU)
    ; at this point the stack must contain the saved RIP/CS/RFLAGS; ensure stack layout matches
    iretq
.no_switch:
    ; restore saved volatile registers and return from interrupt
    pop r11 r10 r9 r8 rdi rsi
    pop rbx rdx rcx rax
    pop rbp
    sti                         ; re-enable interrupts
    iretq