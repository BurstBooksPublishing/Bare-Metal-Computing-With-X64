;; rdi = pointer to capability table entry
    mov     rax, [rdi]           ;; raw pointer to capability word
    cap_from_raw CAP0, rax       ;; HW: synthesize capability register and tag
    ;; CAP0 now holds (base, length, perms, tag). Tag validated by HW.
    ;; Prepare vector length (n = 64 bytes)
    mov     rcx, 64
    ;; CHK_LOAD checks CAP0 and rcx; raises #BR (bounds) on failure.
    chk_load ymm0, CAP0, rcx     ;; HW atomically checks and executes load
    ;; ymm0 now contains 64 bytes starting at CAP0.base + CAP0.offset
    ;; Use ymm0 for AI tensor processing
    vaddps  ymm1, ymm1, ymm0     ;; example SIMD op