; rdi = buffer base, rsi = length
; Create a token in capability register c0: RW, not sealed.
    CTMK c0, rdi, rsi, RIGHTS_RW       ; create token with base/len/rights
; Derive attenuated read-only token in c1
    CTDERIVE c1, c0, RIGHTS_R, 0       ; restrict rights to read-only
; Use c1 to perform a checked read: CTCHECK grants transient access token in ctmp
    CTCHECK ctmp, c1, [rdi + 128]      ; hardware checks bounds and rights
    MOV rax, [ctmp]                    ; read succeeds only if check passed
; Revoke all tokens by bumping epoch (global)
    CTREVOKE                            ; atomic epoch++ and invalidate old tokens