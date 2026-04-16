; rdi -> buffer address, rcx -> number of 64-byte lines
    ; assumes buffer cacheable; caller aligns and sizes appropriately
1:  mov rax, rdi                ; pointer to line
    clwb [rax]                  ; write back cache line to memory (non-temporal)
    add rdi, 64                 ; next line
    dec rcx
    jnz 1b
    sfence                      ; ensure all CLWB reaches memory subsystem
    ; Now the device can DMA the buffer safely.