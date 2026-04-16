#include <stdint.h>
#include <string.h>

struct checkpoint {
    uint64_t gprs[16];         // rax, rbx, rcx, rdx, rsi, rdi, rbp, r8..r15 (order chosen)
    uint64_t rsp;
    uint64_t rip;
    uint64_t rflags;
    uint64_t cr3;
    uint8_t  xsave_area[4096] __attribute__((aligned(64)));
} __attribute__((aligned(64)));

static inline void checkpoint_save(struct checkpoint *cp) {
    // assume cp in %rdi (GCC puts first arg in rdi). Save GPRs and control state.
    asm volatile (
        "movq %%rax, 0(%0)\n\t"     // save RAX
        "movq %%rbx, 8(%0)\n\t"
        "movq %%rcx, 16(%0)\n\t"
        "movq %%rdx, 24(%0)\n\t"
        "movq %%rsi, 32(%0)\n\t"
        "movq %%rdi, 40(%0)\n\t"
        "movq %%rbp, 48(%0)\n\t"
        "movq %%r8, 56(%0)\n\t"
        "movq %%r9, 64(%0)\n\t"
        "movq %%r10, 72(%0)\n\t"
        "movq %%r11, 80(%0)\n\t"
        "movq %%r12, 88(%0)\n\t"
        "movq %%r13, 96(%0)\n\t"
        "movq %%r14,104(%0)\n\t"
        "movq %%r15,112(%0)\n\t"
        "movq %%rsp,128(%0)\n\t"    // save RSP
        "leaq 1f(%%rip), %%rax\n\t" // get current RIP into RAX
        "movq %%rax,136(%0)\n\t"
        "pushfq\n\t"
        "popq %%rax\n\t"
        "movq %%rax,144(%0)\n\t"    // save RFLAGS
        : : "r"(cp) : "memory", "rax"
    );
    // save CR3
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    cp->cr3 = cr3;
    // save XMM/YMM state (FXSAVE area used for portability)
    asm volatile("fxsave (%0)" :: "r"(cp->xsave_area) : "memory");
    return;
    __asm__("1:"); // label referenced for RIP calculation
}

static inline void rollback_restore(struct checkpoint *cp) {
    // restore XMM state first
    asm volatile("fxrstor (%0)" :: "r"(cp->xsave_area) : "memory");
    // restore CR3 to set memory view (flushes TLB)
    asm volatile("mov %0, %%cr3" :: "r"(cp->cr3) : "memory");
    // restore registers and jump to saved RIP; we set RSP before branch.
    asm volatile (
        "movq 0(%0), %%rax\n\t"
        "movq 8(%0), %%rbx\n\t"
        "movq 16(%0), %%rcx\n\t"
        "movq 24(%0), %%rdx\n\t"
        "movq 32(%0), %%rsi\n\t"
        "movq 40(%0), %%rdi\n\t"
        "movq 48(%0), %%rbp\n\t"
        "movq 56(%0), %%r8\n\t"
        "movq 64(%0), %%r9\n\t"
        "movq 72(%0), %%r10\n\t"
        "movq 80(%0), %%r11\n\t"
        "movq 88(%0), %%r12\n\t"
        "movq 96(%0), %%r13\n\t"
        "movq 104(%0), %%r14\n\t"
        "movq 112(%0), %%r15\n\t"
        "movq 128(%0), %%rsp\n\t"      // restore RSP
        "movq 144(%0), %%rflags\n\t"   // restore RFLAGS via push/pop trick not shown; simplified
        "movq 136(%0), %%rax\n\t"
        "jmp *%%rax\n\t"               // jump to saved RIP
        : : "r"(cp) : "memory"
    );
}