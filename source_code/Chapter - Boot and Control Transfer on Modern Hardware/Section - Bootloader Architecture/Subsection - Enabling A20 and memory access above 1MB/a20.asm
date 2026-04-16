; Enable A20: try port 0x92, fallback to 8042 keyboard controller.
; Preserves registers AX,BX,CX,DX,SI,DI,ES and flags.
; Caller: real-mode environment, stack and segments already valid.

enable_a20:
    pushf
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; ---- Fast path: system control port 0x92 ----
    in  al, 0x92           ; read port 0x92
    test al, 2
    jnz .a20_done_fast     ; already enabled
    or   al, 2
    out  0x92, al          ; set A20 bit
    in   al, 0x92
    test al, 2
    jnz .a20_verify        ; success

    ; ---- Fallback: 8042 keyboard controller method ----
    ; Wait for input buffer empty (IBF == 0)
.wait_ibf_clear:
    in  al, 0x64
    test al, 2             ; IBF
    jnz .wait_ibf_clear

    mov al, 0xD0
    out 0x64, al           ; request output port read

.wait_obf_set:
    in  al, 0x64
    test al, 1             ; OBF
    jz  .wait_obf_set

    in  al, 0x60           ; read controller output port
    or  al, 2              ; set A20 bit

    ; Wait IBF clear before sending write command
.wait_ibf_clear2:
    in  al, 0x64
    test al, 2
    jnz .wait_ibf_clear2

    mov al, 0xD1
    out 0x64, al           ; write output port next
    out 0x60, al           ; value with A20 bit set

    ; optional: give controller time (small delay loop)
    mov cx, 1000
.delay:
    loop .delay

.a20_verify:
    ; At this point A20 should be enabled. Optional memory test omitted by default.
    ; If you need to verify, uncomment and set TEST_SEG/TEST_OFF to safe values.
    ;
    ; Example verification (unsafe unless you pick a safe location):
    ; mov ax, 0xFFFF
    ; mov es, ax
    ; mov di, 0x2010         ; maps to physical 0x100000 + 0x2010
    ; mov al, [es:di]        ; read high mem
    ; mov bl, al
    ; mov ax, 0x0000
    ; mov ds, ax
    ; mov si, 0x2010
    ; cmp [ds:si], bl
    ; jne .verification_ok   ; if different then A20 enabled
    ; ; if equal, A20 might still be disabled (wrap), handle as needed

.a20_done_fast:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    ret