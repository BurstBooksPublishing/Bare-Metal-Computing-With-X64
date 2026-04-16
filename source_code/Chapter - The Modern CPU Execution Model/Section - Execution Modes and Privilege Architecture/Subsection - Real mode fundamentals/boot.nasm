org 0x7C00              ; BIOS loads sector here

cli                     ; disable interrupts while we set up
xor ax, ax
mov ds, ax              ; set DS=0
mov es, ax              ; set ES=0
mov ss, ax
mov sp, 0x7C00          ; stack grows down from 0x7C00

mov si, msg             ; SI points to message (offset within this segment)

.print_loop:
    lodsb               ; AL = [DS:SI], SI++
    cmp al, 0
    je .hang
    mov ah, 0x0E        ; BIOS teletype function
    mov bh, 0x00        ; page 0
    mov bl, 0x07        ; text attribute (color)
    int 0x10            ; call BIOS video service
    jmp .print_loop

.hang:
    sti                 ; re-enable interrupts (optional)
    cli
    hlt                 ; halt CPU when done
    jmp .hang

msg: db "Hello, real mode world!", 0

times 510-($-$$) db 0    ; pad to 510 bytes
dw 0xAA55               ; boot signature