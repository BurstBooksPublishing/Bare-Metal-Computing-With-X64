bits 64
section .bss
align 16
df_stack:    resb 4096           ; dedicated IST stack for double fault

section .data
tss:
    ; TSS structure: minimal, only IST[0]..IST[6] used; 104 bytes for 64-bit TSS
    ; Fill other fields with zeros; we set IST1 to point at top of df_stack.
    times 104 db 0

idt_table:
    times 256 dq 0, dq 0        ; 16 bytes per entry; zero-initialize

section .text
global setup_double_fault
extern load_idt   ; implement as lidt wrapper
setup_double_fault:
    ; set IST1 to top of df_stack
    mov rax, df_stack + 4096
    mov qword [tss + 36], rax    ; TSS.ist1 offset (bytes from TSS base)
    ; load TSS descriptor in GDT must exist; here assume gdt_tss_selector is prepared
    mov ax, 0x28                 ; example selector for TSS descriptor
    ltr ax                       ; load task register

    ; create IDT entry for vector 8 (double fault) with ist_index=1
    lea rax, [rel df_handler]
    mov rcx, rax                 ; handler address
    mov rdx, 0x8E                ; type_attr (present, interrupt gate)
    mov bx, 0x10                 ; kernel code selector (example)
    ; pack fields per equation \eqref{eq:idt_pack}
    mov word [idt_table + 8*16 + 0], ax       ; offset_low
    mov word [idt_table + 8*16 + 2], bx       ; selector
    mov byte [idt_table + 8*16 + 4], 1        ; IST index = 1
    mov byte [idt_table + 8*16 + 5], dl       ; type_attr
    shr rax, 16
    mov word [idt_table + 8*16 + 6], ax       ; offset_mid
    shr rax, 16
    mov dword [idt_table + 8*16 + 8], eax     ; offset_high

    ; load IDT: operand is (limit,idt_table)
    lea rax, [rel idt_ptr]
    mov dword [idt_ptr], 16*256 - 1
    mov qword [idt_ptr + 2], idt_table
    lea rax, [rel idt_ptr]
    ; call platform-specific lidt wrapper
    call load_idt
    ret

section .rodata
idt_ptr:    ; 10-byte descriptor for lidt: 2-byte limit + 8-byte base
    times 10 db 0

global df_handler
df_handler:
    cli                           ; disable interrupts
    ; minimal diagnostic: send a byte to COM1 (optional), then reboot
    mov dx, 0x64
    mov al, 0xFE                  ; keyboard controller reset command
.wait:
    out dx, al
    hlt                           ; wait; reset should occur
    jmp .wait