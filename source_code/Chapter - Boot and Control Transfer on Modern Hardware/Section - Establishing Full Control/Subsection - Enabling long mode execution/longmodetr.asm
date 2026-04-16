; Enter long mode: PAE on, load PML4, set LME, enable PG, far jump to 64-bit CS
cli                     ; disable interrupts
lgdt [gdt_descriptor]   ; GDT already contains 64-bit code descriptor at 0x08
; enable PAE (CR4.PAE = bit 5)
mov eax, cr4
or  eax, (1 << 5)       ; set PAE
mov cr4, eax

; load physical base of PML4 into CR3
mov eax, dword [pml4_base]   ; PML4 base must be 4K-aligned and below 4GB here
mov cr3, eax

; enable long mode by setting IA32_EFER.LME (MSR 0xC0000080, bit 8)
mov ecx, 0xC0000080
mov eax, (1 << 8)       ; set LME, clear LMA bit (CPU sets LMA)
xor edx, edx
wrmsr

; enable paging (CR0.PG = bit 31)
mov eax, cr0
or  eax, (1 << 31)      ; set PG
mov cr0, eax

; far jump to selector 0x08 (64-bit code) and label long_mode_entry
jmp 0x08:long_mode_entry

; --- data (defined elsewhere) ---
; gdt_descriptor: dq gdt_limit, gdt_base
; pml4_base: dd pml4_physical_address