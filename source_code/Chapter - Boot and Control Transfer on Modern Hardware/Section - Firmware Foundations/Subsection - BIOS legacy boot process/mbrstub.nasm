bits 16
org 0x7C00

start:
    cli                     ; disable interrupts while we set segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x8000          ; safe stack (physical 0x8000)
    sti                     ; re-enable interrupts

    ; DL contains BIOS boot drive (0x00 floppy, 0x80 first HDD)
    ; Print a message using BIOS teletype (INT 0x10, AH=0x0E)
    mov si, msg
    call print_string

hang:
    jmp hang

print_string:
    lodsb                   ; AL = [DS:SI++]
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07            ; page 0, normal attribute
    int 0x10
    jmp print_string
.done:
    ret

msg db 'Bare-metal MBR demo',0

; pad to 510 bytes, then signature 0xAA55
times 510-($-$$) db 0
dw 0xAA55