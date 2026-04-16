; Registers: rdi = address of data_slot (qword), rsi = data value, rdx = address of flag (qword)
; Producer variant A: use MFENCE to ensure stores are globally ordered.
producer_publish_mfence:
    mov qword [rdi], rsi        ; store data (initialization)
    mfence                      ; ensure the previous store is globally ordered
    mov qword [rdx], 1          ; publish flag (store visible after mfence)
    ret

; Consumer variant A:
consumer_acquire_mfence:
.wait_loop:
    mov rax, qword [rdx]        ; load flag
    test rax, rax
    jz .wait_loop
    mfence                      ; ensure subsequent loads see data written before publish
    mov rbx, qword [rdi]        ; load data (safe)
    ret

; Producer variant B: use LOCK XCHG for publish (RMW provides full fence)
producer_publish_locked:
    mov qword [rdi], rsi        ; store data
    mov rax, 1
    xchg qword [rdx], rax       ; atomic publish; xchg with memory is a full fence
    ret

; Consumer variant B:
consumer_acquire_locked:
.wait_loop2:
    mov rax, qword [rdx]
    test rax, rax
    jz .wait_loop2
    ; no mfence required after locked xchg by producer: the locked RMW enforces ordering
    mov rbx, qword [rdi]        ; safe load of data
    ret