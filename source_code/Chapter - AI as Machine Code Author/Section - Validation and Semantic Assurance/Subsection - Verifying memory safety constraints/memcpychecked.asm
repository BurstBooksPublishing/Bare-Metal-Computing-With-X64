global memcpy_checked
section .text
memcpy_checked:
    ; args: rdi=src, rsi=dst, rdx=n, rcx=dst_base, r8=dst_limit
    cmp rsi, rcx            ; if dst < dst_base -> trap
    jb .trap
    mov rax, rsi
    add rax, rdx            ; rax = dst + n
    cmp rax, r8             ; if dst + n > dst_limit -> trap
    ja .trap
    cld                     ; clear direction flag for REP MOVSB
    xchg rdi, rsi           ; set rsi=src, rdi=dst for REP MOVSB
    rep movsb               ; perform copy (n bytes)
    ret
.trap:
    ud2                     ; undefined instruction -> platform trap