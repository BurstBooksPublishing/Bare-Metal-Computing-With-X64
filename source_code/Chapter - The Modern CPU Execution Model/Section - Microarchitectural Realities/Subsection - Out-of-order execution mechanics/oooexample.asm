.intel_syntax noprefix
    .section .text
    .global _start
_start:
    ; A chain that would create false WAW/WAR without renaming.
    mov rax, rcx         ; 1: read rcx -> arch RAX
    add rax, 1           ; 2: produce new RAX (renamed)
    mov rax, rax         ; 3: trivial write after write (WAW)
    ; With renaming, physical destinations avoid serialization.

    ; Store followed by speculative load to same address.
    lea rdi, [rip+buf]   ; address in RDI
    mov qword ptr [rdi], rax  ; store (younger store in program order)
    mov rbx, qword ptr [rdi]   ; load possibly executed early (younger->older)
    ; Microarchitectures may forward from store buffer to this load.
    ; If addresses alias partially or alignment differs, forwarding may fail,
    ; causing load replay or pipeline flush.

    ; Exit (syscall)
    mov rax, 60          ; sys_exit
    xor rdi, rdi
    syscall

    .section .data
buf: .quad 0