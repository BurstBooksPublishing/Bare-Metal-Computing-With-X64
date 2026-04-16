; assume: GDT configured with 64-bit code/data descriptors
cli                             ; disable interrupts
; set CR4.PAE
mov rax, cr4                    ; read CR4
or  rax, (1 << 5)               ; set PAE bit
mov cr4, rax                    ; write CR4

; set IA32_EFER.LME (MSR 0xC0000080)
mov ecx, 0xC0000080             ; MSR index IA32_EFER
mov eax, (1 << 8)               ; LME = 1 in low dword
xor edx, edx                    ; high dword = 0
wrmsr                           ; write MSR (CPL0 required)

; load CR3 with physical PML4 base (must be 4KiB-aligned)
mov rax, PML4_BASE               ; physical address of PML4
mov cr3, rax

; enable paging by setting CR0.PG
mov rax, cr0
or  rax, (1 << 31)              ; set PG
mov cr0, rax

; far jump to reload CS with 64-bit code segment (GDT selector 0x08)
jmp 0x08:long_mode_entry