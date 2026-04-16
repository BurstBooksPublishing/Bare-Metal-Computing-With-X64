.intel_syntax noprefix
.global hot_branch_unaligned, hot_branch_aligned

# Unaligned hot branch (no explicit alignment)
hot_branch_unaligned:
    cmp rdi, rsi            # compare arguments; branch depends on result
    je short_target_unaligned
    # fall-through: cold path
    mov eax, 0
    ret
short_target_unaligned:
    mov eax, 1              # hot taken path
    ret

# Aligned hot branch target placed on 32-byte boundary
.p2align 5, 0x90            # align next label to 2^5 = 32 bytes, pad with NOPs
hot_branch_aligned:
    cmp rdi, rsi
    je short_target_aligned
    mov eax, 0
    ret
short_target_aligned:
    mov eax, 1
    ret