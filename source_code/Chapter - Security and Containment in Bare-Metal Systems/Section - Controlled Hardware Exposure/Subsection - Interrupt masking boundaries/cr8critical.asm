.intel_syntax noprefix
    ; Enter privileged critical region: raise TPR to 0xF (highest threshold).
    mov     rax, 0xF          ; set threshold value in register (only low 4 bits used)
    mov     cr8, rax          ; privileged write: updates local APIC TPR
    mfence                   ; serializing memory before critical section

    ; -- critical machine-code region here --
    ; deterministic operations without maskable interrupt delivery

    mfence                   ; ensure stores visible before lowering threshold
    xor     rax, rax          ; clear rax
    mov     cr8, rax          ; restore TPR to 0 (allow all deliveries)
    ; return to normal execution