; Entering IA-32e: protected mode must be active when this runs.
; pml4_phys: 64-bit physical address of PML4 table (4K-aligned).

    ; load CR3 with PML4 base (eax holds low 32 bits, edx high on x86-64; in 32-bit protected with PAE only low 32 bits used)
    mov     eax, dword [pml4_phys]    ; low 32 bits of PML4 physical base
    xor     edx, edx                  ; high 32 bits zero for typical systems under 40-bit physical
    mov     cr3, eax                  ; set page-table root

    ; enable PAE in CR4 (set bit 5)
    mov     eax, cr4
    or      eax, 1<<5                 ; CR4.PAE = 1
    mov     cr4, eax

    ; enable LME by writing IA32_EFER MSR (MSR 0xC0000080)
    mov     ecx, 0xC0000080
    rdmsr                              ; EDX:EAX <- MSR
    or      eax, 1<<8                  ; set LME (bit 8)
    xor     edx, edx                   ; preserve EDX if needed; here high bits clear
    wrmsr                              ; commit IA32_EFER

    ; enable paging by setting CR0.PG (bit 31)
    mov     eax, cr0
    or      eax, 1<<31                 ; CR0.PG = 1
    mov     cr0, eax

    ; serialize and perform far jump to flush prefetch and load 64-bit CS
    ljmp    $0x08, $long_mode_entry    ; selector 0x08 is 64-bit kernel code descriptor in GDT

long_mode_entry:
    ; execution now in IA-32e mode with long mode active.
    ; continue with 64-bit setup (stack, registers) and enable paging specifics.