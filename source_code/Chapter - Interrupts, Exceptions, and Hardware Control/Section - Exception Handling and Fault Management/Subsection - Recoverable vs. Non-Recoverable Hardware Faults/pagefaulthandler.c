#include <stdint.h>

/* Minimal saved-register frame passed from assembly stub. */
struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rip, cs, rflags, rsp, ss; /* CPU-pushed frame end */
};

/* External platform primitives (provided elsewhere). */
extern void *allocate_phys_page(void);          /* allocate one physical page */
extern int map_page(uint64_t linear, void *phys, uint64_t flags); /* install PTE */

/* Returns 1 if handled and execution may resume; 0 otherwise. */
int page_fault_handler(struct regs *r, uint64_t error_code) {
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr)); /* read CR2 */

    /* If the page was present but protection violated, don't auto-fix. */
    if (error_code & 0x1) return 0; /* present bit set -> protection fault */

    /* Allocate a physical page and map it read/write/user. */
    void *phys = allocate_phys_page(); /* may block or fail in a constrained kernel */
    if (!phys) return 0; /* cannot satisfy fault */

    /* Map with suitable flags; return non-zero on success. */
    if (!map_page(fault_addr & ~0xFFFULL, phys, /*flags=*/0x3 /*RW, user*/))
        return 0;

    /* Handler succeeded; assembly stub will iretq and resume at r->rip. */
    return 1;
}