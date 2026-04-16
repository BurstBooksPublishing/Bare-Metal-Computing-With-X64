BITS 64
global kernel_entry
section .text

; Entry point in long mode. Stack must be set by bootloader.
kernel_entry:
    cli                             ; mask interrupts
    ; CPUID: check for XSAVE and AVX support (leaf 1)
    xor rax, rax
    mov eax, 1
    cpuid
    test ecx, 1 << 26               ; ECX.OSXSAVE
    jz .no_xsave
    test ecx, 1 << 28               ; ECX.AVX
    jz .no_xsave

    ; Enable OSXSAVE in CR4 (bit 18)
    mov ecx, cr4
    or  ecx, (1 << 18)
    mov cr4, ecx

    ; Enable XMM and YMM state in XCR0 (bits 0 and 1 => value 3)
    xor rax, rax
    xor rdx, rdx
    mov eax, 3
    ; xsetbv writes to XCR0; requires ECX=0
    xor ecx, ecx
    xsetbv

    ; Initialize FP/SIMD control for deterministic behavior
    ; Set MXCSR: clear exceptions, set round-to-nearest (0)
    mov eax, 0x1f80                 ; default MXCSR with exceptions masked
    stmxcsr [rel mxcsr_save]
    mov dword [rel mxcsr_save], eax
    ldmxcsr [rel mxcsr_save]

    ; Mask legacy PIC (8259): port 0x21 and 0xA1 -> 0xFF (all masked)
    mov al, 0xFF
    out 0x21, al
    out 0xA1, al

    ; Minimal IDT setup: load an IDT with empty handlers to trap faults deterministically.
    ; (IDT data must be prepared in .data; omitted here for brevity)

    ; Kernel main loop: call deterministic AI microkernel function pointer in rdi
.loop:
    ; rdi = pointer to inference routine, rsi = context pointer
    call rdi                        ; invoke microkernel (must be carefully audited)
    hlt                             ; halt until explicit resume by hardware/watchdog
    jmp .loop

.no_xsave:
    ; CPU lacks required features; hang deterministically.
    mov rdi, err_msg
    call print_string               ; minimal serial print (implementation elsewhere)
    jmp .

section .data
mxcsr_save: dd 0
err_msg:    db "XSAVE/AVX unsupported",0