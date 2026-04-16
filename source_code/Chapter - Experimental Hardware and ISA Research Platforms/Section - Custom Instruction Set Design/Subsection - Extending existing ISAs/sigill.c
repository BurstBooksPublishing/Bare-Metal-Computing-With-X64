#define _GNU_SOURCE
#include <signal.h>
#include <ucontext.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const uint8_t custom_prefix[3] = {0x0F, 0x3A, 0xFF}; // research escape

// read a byte safely at address p
static inline int safe_read_byte(void *p, uint8_t *out) {
    volatile uint8_t *b = (volatile uint8_t*)p;
    *out = *b;
    return 0;
}

static void sigill_handler(int sig, siginfo_t *si, void *uc_void) {
    ucontext_t *uc = (ucontext_t*)uc_void;
    // use POSIX ucontext register indices
    greg_t *gregs = uc->uc_mcontext.gregs;
    uintptr_t rip = gregs[REG_RIP];
    uint8_t b;
    // check prefix bytes
    for (int i = 0; i < 3; ++i) {
        if (safe_read_byte((void*)(rip + i), &b)) { goto deliver; }
        if (b != custom_prefix[i]) { goto deliver; }
    }
    // read ModR/M
    uint8_t modrm;
    if (safe_read_byte((void*)(rip + 3), &modrm)) { goto deliver; }
    if ((modrm >> 6) != 3) { goto deliver; } // only register-direct supported
    int reg = (modrm >> 3) & 7; // destination register index
    int rm  = modrm & 7;       // source register index

    // Map 3-bit indices to x86_64 general-purpose registers in ucontext
    // mapping: 0->RAX,1->RCX,2->RDX,3->RBX,4->RSP,5->RBP,6->RSI,7->RDI
    static const int idx_map[8] = {REG_RAX, REG_RCX, REG_RDX, REG_RBX,
                                   REG_RSP, REG_RBP, REG_RSI, REG_RDI};
    greg_t val_dest = gregs[idx_map[reg]];
    greg_t val_src  = gregs[idx_map[rm]];

    // semantics: dest := dest + src (64-bit)
    gregs[idx_map[reg]] = (greg_t)(val_dest + val_src);

    // advance RIP by 4 bytes (escape + modrm)
    gregs[REG_RIP] = rip + 4;
    return;

deliver:
    // not our instruction: restore default and re-raise
    struct sigaction sa = { .sa_handler = SIG_DFL };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGILL, &sa, NULL);
}

int install_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigill_handler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    if (sigaction(SIGILL, &sa, NULL) != 0) return -1;
    return 0;
}

/* The test harness would emit the 4-byte custom instruction
   into executable memory and jump to it. Omitted for brevity. */
int main(void) {
    if (install_handler() != 0) { perror("sigaction"); return 1; }
    puts("Handler installed. Execute custom opcode from test stub.");
    /* load and run test stub that uses custom prefix ... */
    return 0;
}