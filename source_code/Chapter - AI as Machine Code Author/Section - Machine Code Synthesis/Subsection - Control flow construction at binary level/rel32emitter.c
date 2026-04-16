#include 
#include 
#include 
#include 
#include 
#include 
#include 

// Serialize stores so instruction fetch sees them (mfence; cpuid).
static inline void cpu_serialize(void) {
    asm volatile("mfence" ::: "memory");
    uint32_t eax=0, ebx=0, ecx=0, edx=0;
    asm volatile("cpuid"
                 : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx)
                 : /* none */ : "memory");
}

// Write a rel32 jmp at buf (caller ensures buf maps to src_addr).
// Returns bytes written (5) if direct, otherwise 0.
size_t emit_rel32_jmp(uint8_t *buf, size_t buf_size, uint64_t src_addr, uint64_t dst_addr) {
    if (buf_size < 5) return 0;
    int64_t disp = (int64_t)dst_addr - ((int64_t)src_addr + 5);
    if (disp < INT32_MIN || disp > INT32_MAX) return 0;
    buf[0] = 0xE9; // opcode
    int32_t d32 = (int32_t)disp;
    memcpy(buf + 1, &d32, sizeof(d32)); // little-endian
    cpu_serialize();
    return 5;
}

// Allocate a trampoline that jumps to 'target' using movabs rax,imm64; jmp rax.
// Returns pointer to trampoline (RX) or NULL on failure.
void *alloc_trampoline(uint64_t target) {
    const size_t tramp_size = 12; // REX.W + B8 + imm64 (10) + jmp rax (2)
    size_t pagesz = (size_t)sysconf(_SC_PAGESIZE);
    size_t alloc = (tramp_size + pagesz - 1) & ~(pagesz - 1);
    void *mem = mmap(NULL, alloc, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return NULL;
    uint8_t *p = (uint8_t*)mem;
    // mov rax, imm64  => REX.W (0x48), opcode B8 + rd(0) => 0x48 0xB8 imm64
    p[0] = 0x48; p[1] = 0xB8;
    memcpy(p + 2, &target, 8);
    // jmp rax => FF E0
    p[10] = 0xFF; p[11] = 0xE0;
    cpu_serialize();
    return mem;
}

// Example usage: patch location 'site' (at address site_addr) to branch to dst.
// If rel32 fits, emit direct; otherwise allocate trampoline and write a rel32 to it.
int patch_branch(uint8_t *site, uint64_t site_addr, uint64_t dst_addr) {
    size_t wrote = emit_rel32_jmp(site, 5, site_addr, dst_addr);
    if (wrote == 5) return 0;
    void *tr = alloc_trampoline(dst_addr);
    if (!tr) return -1;
    uint64_t tr_addr = (uint64_t)tr;
    // Now try to encode rel32 to trampoline (should be in range if allocator near).
    if (emit_rel32_jmp(site, 5, site_addr, tr_addr) != 5) return -2;
    return 0;
}