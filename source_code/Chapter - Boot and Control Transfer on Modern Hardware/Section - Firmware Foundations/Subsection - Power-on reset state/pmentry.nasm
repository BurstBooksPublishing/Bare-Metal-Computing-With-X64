[BITS 16]
org 0xFFF0                      ; logical origin where firmware is mapped near reset

start:
    cli                         ; ensure interrupts disabled (IF=0)
    call enable_a20             ; platform-specific; ensure linear addressing above 1MiB
    call setup_gdt              ; place GDT at a known, aligned RAM location
    lgdt [gdt_descriptor]       ; load GDT register with our table descriptor
    mov eax, cr0
    or  eax, 1                  ; set PE (protected enable) bit
    mov cr0, eax                ; enable protected mode
    jmp 0x08:pm_entry32         ; far jump: selector 0x08 in GDT for code segment

[BITS 32]
pm_entry32:
    ; now executing in protected mode with 32-bit default operand size
    ; set up stack, enable paging later as desired
    mov eax, 0x10               ; data selector index (0x10) from GDT
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x9FC00            ; example stack location below 1MiB before further init
    ; continue platform initialization...

; --- data and descriptors ---
align 8
gdt_start:
    dq 0x0000000000000000       ; null descriptor
gdt_code:                       ; base=0 limit=4GiB, code segment
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A                       ; code: execute/read, ring 0
    db 0xCF
    db 0x00
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92                       ; data: read/write, ring 0
    db 0xCF
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; limit
    dd gdt_start                ; base (32-bit low); in 64-bit systems, loader must store full base in 64-bit IDTR if needed

; --- stubs ---
enable_a20:
    ret                         ; placeholder: real firmware toggles A20 via chipset IO

setup_gdt:
    ret                         ; placeholder: may relocate GDT to RAM that is writeable