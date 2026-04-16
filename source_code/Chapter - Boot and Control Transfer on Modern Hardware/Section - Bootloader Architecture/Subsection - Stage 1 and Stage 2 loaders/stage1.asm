; ORG places code at the address the BIOS loads the sector (0x7C00).
[org 0x7C00]

start:
    cli                     ; disable interrupts during setup
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; set up a small stack in low memory
    mov ss, ax
    mov sp, 0x7C00          ; stack grows down from 0x7C00

    ; load 2 sectors (stage2) using BIOS int 0x13 into 0x0000:0x8000
    mov bx, 0x8000          ; offset within segment 0x0000
    mov dh, 0x00            ; head (unused for LBA)
    mov dl, [BOOT_DRIVE]    ; BIOS boot drive (passed by BIOS at 0x7C00+0x1FE)
    mov si, disk_retries
load_sectors:
    mov ah, 0x02            ; INT 13h, function 02h (read sectors)
    mov al, 2               ; read 2 sectors (extend as needed)
    mov ch, 0               ; cylinder low
    mov cl, 2               ; sector (start at 2 for multistage; adjust as layout)
    mov dh, 0               ; head
    mov es, 0x0000
    int 0x13
    jc disk_error
    jmp loaded

disk_error:
    dec si
    jz halt
    ; brief delay and retry
    mov cx, 0xFFFF
.wait:  loop .wait
    jmp load_sectors

loaded:
    sti                     ; re-enable interrupts if desired
    ; prepare registers for stage2: set DS/ES to 0x0000 and jump
    xor ax, ax
    mov ds, ax
    mov es, ax
    ; Far jump to 0x0000:0x8000 (physical 0x8000)
    jmp 0x0000:0x8000

halt:
    hlt                     ; unrecoverable failure

; Data
BOOT_DRIVE:    db 0        ; filled by BIOS: DL at entry (optional copy)
disk_retries:  db 3

; Pad so the sector ends with boot signature at 0x1FE--0x1FF
times 510-($-$$) db 0
dw 0xAA55                ; boot signature