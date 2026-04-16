BITS 16
org 0x7c00

start:
    cli                         ; disable interrupts
    lgdt [gdt_descriptor]       ; load GDT pointer
    mov eax, cr0
    or  eax, 1                  ; set PE bit
    mov cr0, eax
    jmp 0x08:protected_entry    ; far jump to load CS (selector 0x08)

gdt_descriptor:
    dw gdt_end - gdt - 1        ; limit
    dd gdt                      ; base

gdt:
    dq 0x0000000000000000       ; null descriptor
    ; code descriptor: base=0x00000000, limit=0xFFFFF, G=1, D/B=1, exec/read, DPL=0, P=1
    dw 0xFFFF                   ; limit low
    dw 0x0000                   ; base low
    db 0x00                     ; base mid
    db 0x9A                     ; type=0xA, S=1, DPL=0, P=1 (code)
    db 0xCF                     ; limit high=0xF, G=1, D/B=1
    db 0x00                     ; base high
    ; data descriptor: base=0x00000000, limit=0xFFFFF, G=1, D/B=1, read/write
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92                     ; type=0x2, S=1, DPL=0, P=1 (data)
    db 0xCF
    db 0x00
gdt_end:

; 32-bit protected-mode code entry
[BITS 32]
protected_entry:
    mov ax, 0x10                ; data selector (index 2 -> 0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9FC00            ; establish stack
    sti                         ; enable interrupts (optional after IDT)
    ; now in protected mode with flat segments; next: setup IDT, TSS, paging
    cli
    hlt                         ; placeholder: replace with real kernel entry