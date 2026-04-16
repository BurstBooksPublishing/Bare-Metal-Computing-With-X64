; eax/edx/ecx used for WRMSR; pml4_phys holds physical address of PML4
    mov     eax, dword pml4_phys        ; low 32 bits of PML4 physical base
    mov     edx, 0                      ; high 32 bits (assume <4GB)
    mov     cr3, eax                    ; load page-table base (CR3)

    ; enable PAE: set CR4.PAE (bit 5)
    mov     eax, cr4
    or      eax, 1 << 5                 ; CR4 |= 0x20
    mov     cr4, eax

    ; enable LME in IA32_EFER (MSR 0xC0000080): set bit 8
    mov     ecx, 0xC0000080             ; MSR index IA32_EFER
    mov     eax, 1 << 8                 ; EFER.LME = 1
    mov     edx, 0
    wrmsr                               ; write IA32_EFER

    ; enable paging: set CR0.PG (bit 31)
    mov     eax, cr0
    or      eax, 1 << 31                ; CR0 |= 0x80000000
    mov     cr0, eax

    ; far jump into 64-bit code selector (GDT must provide L-bit set descriptor at 0x08)
    jmp     0x08:long_mode_entry         ; load CS and begin 64-bit execution

; pml4_phys defined elsewhere as constant or label resolved to physical base