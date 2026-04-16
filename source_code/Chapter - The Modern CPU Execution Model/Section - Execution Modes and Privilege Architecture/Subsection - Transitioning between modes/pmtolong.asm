; CLI to avoid interrupts during transition
cli

; Enable PAE: CR4.PAE = 1
mov eax, cr4                ; read CR4
or  eax, 0x20               ; mask_PAE = 1<<5
mov cr4, eax                ; write CR4

; Ensure CR3 points to PML4 (physical address must be page-aligned)
mov eax, dword [pml4_phys]  ; low 32 bits of PML4 phys addr
mov cr3, eax                ; load CR3

; Set EFER.LME via WRMSR (MSR 0xC0000080)
mov ecx, 0xC0000080
rdmsr                       ; RDX:RAX <- MSR
or  eax, 0x100              ; set LME = 1
wrmsr                       ; write back

; Enable paging: CR0.PG = 1
mov eax, cr0
or  eax, 0x80000000         ; mask_PG = 1<<31
mov cr0, eax

; Now processor is in long mode conceptually; perform far jump to 64-bit CS
; Selector 0x08 must point to a 64-bit code descriptor in the GDT.
jmp 0x08:long_mode_entry    ; far jump to load 64-bit CS and enter 64-bit semantics

; ---------------- Data ----------------
align 4096
pml4_phys: dd 0x00000000     ; low 32 bits of PML4 physical address (example)
           dd 0x00000000     ; high 32 bits if needed