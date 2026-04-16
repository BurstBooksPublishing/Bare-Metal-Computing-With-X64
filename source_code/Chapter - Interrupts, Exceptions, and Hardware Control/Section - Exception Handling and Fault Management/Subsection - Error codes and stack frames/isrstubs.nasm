BITS 64
global isr_noerr, isr_err

; Entry used for interrupts/exceptions without hardware error code.
isr_noerr:
    push 0                  ; synthesize error code (8 bytes)
    jmp isr_common

; Entry used for exceptions that hardware pushes an error code for.
isr_err:
    jmp isr_common

; Common prologue: save general registers and call dispatcher.
isr_common:
    ; Save volatile and callee-saved regs in a documented order (15 regs -> 120 bytes).
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
    ; Now [rsp] .. [rsp+120) contain saved registers; error code is at [rsp + 120].
    mov rdi, rsp            ; first arg: pointer to saved-frame (frame base)
    mov rsi, [rsp + 120]    ; second arg: error code (low 32 bits, zero-extended)
    mov rdx, qword [rip + isr_vector_id] ; third arg (optional): vector id (runtime fixed)
    ; Call C dispatcher (clobbers rax, etc.; saved regs preserved on stack).
    extern exception_dispatch
    call exception_dispatch
    ; Restore registers (reverse order).
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    iretq                   ; return from interrupt/exception
; placeholder memory containing vector id may be patched per entry.
isr_vector_id: dq 0