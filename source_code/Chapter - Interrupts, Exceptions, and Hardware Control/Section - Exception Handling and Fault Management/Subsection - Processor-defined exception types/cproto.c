struct regs { uint64_t rip, cs, rflags, rsp, ss; uint64_t gprs[11]; /* rax,rbx,... */ };
void common_exception_handler(uint64_t vector, uint64_t error_code, struct regs *state);