BITS 64
global _start
section .text

_start:
    ; --- Dependent chain: each add waits on previous result.
    mov rax, 0          ; accumulator
    mov rbx, 1
    add rax, rbx        ; depends on rax
    add rax, rbx        ; serial dependence
    add rax, rbx
    add rax, rbx

    ; --- Independent adds: four ALUs can execute concurrently.
    mov r8,  0
    mov r9,  0
    mov r10, 0
    mov r11, 0
    add r8,  rbx        ; independent registers r8..r11
    add r9,  rbx
    add r10, rbx
    add r11, rbx
    add rax, r8         ; reduce partial sums
    add rax, r9
    add rax, r10
    add rax, r11

    ; exit syscall (Linux x86-64)
    mov rax, 60         ; sys_exit
    xor rdi, rdi
    syscall