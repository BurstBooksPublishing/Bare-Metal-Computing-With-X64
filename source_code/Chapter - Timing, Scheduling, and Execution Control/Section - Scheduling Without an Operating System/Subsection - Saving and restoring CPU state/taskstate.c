struct task_state {
    uint64_t gpr[16];      // RAX,RBX,RCX,RDX,RSI,RDI,RBP,RSP,R8..R15
    uint64_t rip;          // saved RIP
    uint64_t rflags;       // saved RFLAGS
    uint64_t cr3;          // page-table base
    alignas(16) uint8_t fxsave[512]; // FXSAVE area (512 bytes)
    // other optional fields: fs_base, gs_base, selected MSRs...
};