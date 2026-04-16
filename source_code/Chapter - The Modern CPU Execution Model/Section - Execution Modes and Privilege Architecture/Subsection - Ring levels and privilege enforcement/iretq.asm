; Inputs:
    ;   rcx = user RIP
    ;   rdx = user RSP (stack top in user space)
    ; Registers preserved by caller convention are saved as needed.

    pushq   $0x23                # user SS (RPL=3, data selector)
    pushq   rdx                  # user RSP
    pushfq                       # push RFLAGS (interrupt enable/disable preserved)
    pushq   $0x1B                # user CS (RPL=3, code selector)
    pushq   rcx                  # user RIP (entry point in user space)
    iretq                        # hardware pops RIP/CS/RFLAGS/ RSP/SS and returns to ring 3

    ; Execution continues in user mode at (CS:RIP) with SS:RSP set and RFLAGS restored.