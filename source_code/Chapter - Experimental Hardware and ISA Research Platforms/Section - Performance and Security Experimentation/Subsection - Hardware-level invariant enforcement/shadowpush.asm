; rdi: pointer to shadow stack top (virtual address)
; rax: scratch (will hold PKRU)
; rcx, rdx: must be zero per WRPKRU convention
; rbx: return address to push
push_shadow:
    xor rcx, rcx                ; ECX=0 for WRPKRU
    xor rdx, rdx                ; EDX=0 for WRPKRU
    rdpkru                      ; EAX := PKRU (read current rights)
    mov ebx, eax                ; save original low 32 bits
    ; set access bits for PKEY3 to allow write (bits layout: two bits per PKEY)
    ; compute new PKRU value with PKEY3 write-enable cleared (0 enables)
    ; here we flip only PKEY3's AD/WD bits; constant mask precomputed in kernel
    mov eax, ebx
    and eax, 0xFFFFF8FF         ; clear AD/WD bits for PKEY3 (example mask)
    wrpkru                      ; update PKRU -> shadow stack pages writable
    mov rax, [rsp]              ; obtain return address pushed by CALL (RIP saved on stack)
    mov [rdi], rax              ; write to shadow stack (now writable)
    ; restore original PKRU
    mov eax, ebx
    wrpkru
    ret