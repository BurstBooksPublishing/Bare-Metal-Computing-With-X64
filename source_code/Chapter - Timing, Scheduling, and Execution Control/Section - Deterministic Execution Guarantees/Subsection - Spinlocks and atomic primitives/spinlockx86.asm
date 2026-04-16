global spin_lock, spin_unlock, spin_trylock

section .text
; void spin_lock(uint64_t *lock)
spin_lock:
    mov rax, 1               ; value to set (locked)
1:  xchg rax, qword [rdi]    ; atomic: rax <- [rdi], [rdi] <- 1
    test rax, rax            ; was previous value zero?
    jz .locked               ; if zero, we acquired the lock
.spin_wait:
    pause                    ; reduce pipeline pressure
    mov rax, qword [rdi]     ; read current state
    test rax, rax
    jne .spin_wait           ; still locked; continue waiting
    mov rax, 1
    jmp 1b                   ; try atomic exchange again
.locked:
    ret

; int spin_trylock(uint64_t *lock) -> returns 0 on success, 1 if locked
spin_trylock:
    mov rax, 1
    xchg rax, qword [rdi]    ; atomic attempt
    test rax, rax
    setnz al                  ; al = 1 if previously nonzero (failure)
    movzx rax, al
    ret

; void spin_unlock(uint64_t *lock)
spin_unlock:
    mov qword [rdi], 0       ; release: plain store provides release semantics on x86
    sfence                   ; optional: ensure device-visible ordering if needed
    ret