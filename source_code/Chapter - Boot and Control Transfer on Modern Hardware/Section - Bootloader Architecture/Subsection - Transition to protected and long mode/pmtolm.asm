; Entered with GDT installed and selector CODE32_SELECTOR active.
cli                         ; disable interrupts
; Set CR4.PAE = 1
mov eax, cr4
or eax, 0x20                ; PAE bit
mov cr4, eax

; Load CR3 with PML4 physical base (must be aligned).  Replace pml4_phys.
mov eax, pml4_phys          ; low 32 bits of PML4 base
mov cr3, eax

; Enable LME in IA32_EFER MSR (0xC0000080)
mov ecx, 0xC0000080
xor edx, edx
mov eax, 0x100              ; LME bit (bit 8)
wrmsr                      ; enable LME

; Enable paging by setting CR0.PG
mov eax, cr0
or eax, 0x80000000          ; PG bit
mov cr0, eax

; Far jump into long-mode 64-bit code selector. Selector must index a GDT
; entry whose L bit is prepared.
jmp CODE64_SELECTOR:long_mode_entry

; -------------------------
; long_mode_entry executes in 64-bit mode (RIP state).
; Set up 64-bit stack, long-mode FS/GS if desired, and continue.