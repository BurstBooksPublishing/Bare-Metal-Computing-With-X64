bits 32
; Assumes: running in protected mode, CR0.PE=1, interrupts disabled.

SECTION .data
ALIGN 4096
pml4:      times 512 dq 0               ; 4 KiB PML4 table
ALIGN 4096
pdpt:      times 512 dq 0               ; 4 KiB PDPT table
ALIGN 4096
pd0:       times 512 dq 0               ; 4 KiB PD for 2MiB pages

SECTION .text
global setup_identity_paging
setup_identity_paging:
    ; PML4[0] = addr(pdpt) | P|RW
    mov eax, pdpt
    or  eax, 0x03
    mov dword [pml4], eax
    mov dword [pml4 + 4], 0

    ; PDPT[0] = addr(pd0) | P|RW
    mov eax, pd0
    or  eax, 0x03
    mov dword [pdpt], eax
    mov dword [pdpt + 4], 0

    ; Fill pd0: PD[i] = (i * 2MiB) | P|RW|PS
    xor esi, esi                    ; index i = 0
    mov edi, 0x200000               ; 2 MiB increment
.fill_pd_loop:
    mov eax, esi
    imul eax, edi                   ; eax = i * 2MiB
    or  eax, 0x83                   ; P(1) | RW(2) | PS(0x80)
    mov [pd0 + esi*8], eax          ; lower dword of qword entry
    mov dword [pd0 + esi*8 + 4], 0  ; upper dword zero (covers <4GiB)
    inc esi
    cmp esi, 512
    jl .fill_pd_loop

    ; Load CR3 with PML4 physical base (low 32 bits sufficient if below 4GiB)
    mov eax, pml4
    mov cr3, eax

    ; Enable PAE: CR4.PAE = bit 5
    mov eax, cr4
    or  eax, 0x20
    mov cr4, eax

    ; Enable long mode (EFER.LME): IA32_EFER MSR = 0xC0000080, LME = bit 8
    mov ecx, 0xC0000080
    mov eax, 1 << 8
    xor edx, edx
    wrmsr

    ; Enable paging: CR0.PG = bit 31
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    ; Far jump to flush pipeline and switch to long-mode CS (selector must be set up)
    ; Example: jmp 08h:long_mode_entry  ; 0x08 is placeholder selector for 64-bit code
    ret