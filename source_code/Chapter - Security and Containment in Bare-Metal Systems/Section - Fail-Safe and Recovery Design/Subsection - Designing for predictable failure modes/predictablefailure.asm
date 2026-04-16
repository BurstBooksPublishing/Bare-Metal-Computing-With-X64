global safe_reboot
section .text

; rdi = physical/identity-mapped base pointer for snapshot_area
safe_reboot:
    push rbp
    mov rbp, rsp
    ; magic marker
    mov qword [rdi + 0x00], 0xC0DEF00DCAFEBABE
    ; save general-purpose registers (current values)
    mov [rdi + 0x08], rax
    mov [rdi + 0x10], rbx
    mov [rdi + 0x18], rcx
    mov [rdi + 0x20], rdx
    mov [rdi + 0x28], rsi
    mov [rdi + 0x30], rdi
    mov [rdi + 0x38], rbp
    mov [rdi + 0x40], rsp
    mov [rdi + 0x48], r8
    mov [rdi + 0x50], r9
    mov [rdi + 0x58], r10
    mov [rdi + 0x60], r11
    mov [rdi + 0x68], r12
    mov [rdi + 0x70], r13
    mov [rdi + 0x78], r14
    mov [rdi + 0x80], r15
    ; capture RIP
    call .Lrip
.Lrip:
    pop rax
    mov [rdi + 0x88], rax
    ; capture RFLAGS
    pushfq
    pop rax
    mov [rdi + 0x90], rax
    ; compute simple XOR checksum across first 12 qwords
    xor rax, rax
    mov rcx, 12
    lea rsi, [rdi]
    xor rdx, rdx
.chk_loop:
    mov rbx, [rsi + rdx*8]
    xor rax, rbx
    inc rdx
    cmp rdx, rcx
    jl .chk_loop
    mov [rdi + 0xA0], rax
    ; request chipset soft-reset via port 0xCF9
    mov dx, 0xCF9           ; reset control port
    mov al, 0x06            ; assert SYS_RESET#
    out dx, al
    hlt                     ; fallback if reset fails