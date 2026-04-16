; rdx = pointer to domain descriptor struct
; rax = physical PML4 root for domain
; rcx = domain entry rip (virtual)
; r8  = domain initial stack pointer
; r9  = PKRU value for domain

    mov     rax, [rdx + 0]        ; load PML4 physical root into rax
    mov     ecx, dword [rdx + 8]  ; load entry RIP low dword (example)
    mov     r8,  [rdx + 16]       ; load domain stack pointer
    mov     eax, dword r9         ; PKRU low 32 bits in eax

    ; install fast domain pointer into GS base for quick checks
    mov     rax, rdx
    wrgsbase rax                  ; write GS.base = domain descriptor pointer

    ; install PKRU (EAX holds new PKRU value), ECX=0, EDX=0 per spec
    xor     ecx, ecx
    xor     edx, edx
    wrpkru                        ; privileged instruction effect applied

    ; install page-table root (CR3 <- rax), atomic w.r.t. DMU checks
    mov     cr3, rax

    ; set up stack and jump to domain entry
    mov     rsp, r8
    lfence                        ; serialize previous writes
    jmp     rcx                   ; enter domain