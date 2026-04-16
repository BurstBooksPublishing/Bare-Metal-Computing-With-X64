#include 
#include 

#define PAGE_SIZE 4096ULL
#define PTE_PRESENT  (1ULL<<0)

static inline void invlpg(void *addr) {
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

/* CPUID leaf 1: check ECX bit 30 for RDRAND support */
static inline bool cpu_has_rdrand(void) {
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(1), "c"(0));
    return (ecx >> 30) & 1;
}

/* Try one RDRAND 64-bit; return true on success */
static inline bool rdrand64(uint64_t *out) {
    unsigned char ok;
    asm volatile("rdrand %0; setc %1"
                 : "=r"(*out), "=qm"(ok));
    return ok;
}

/* Fallback PRNG: xorshift64* seeded from TSC and address mixing */
static inline uint64_t fallback_prng(uint64_t seed) {
    uint64_t x = seed ? seed : 0x9e3779b97f4a7c15ULL;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    return x * 0x2545F4914F6CDD1DULL;
}

/* Generate a non-zero 64-bit canary */
uint64_t generate_canary(void) {
    uint64_t r = 0;
    if (cpu_has_rdrand()) {
        if (rdrand64(&r) && r != 0) return r;
    }
    uint64_t tsc;
    asm volatile("rdtsc" : "=A"(tsc)); /* legacy; produces EDX:EAX in GCC */
    r = fallback_prng(tsc ^ (uintptr_t)&generate_canary);
    if (r == 0) r = 0xdeadbeefcafebabeULL;
    return r;
}

/* Initialize a per-boot canary stored at canary_ptr */
void init_stack_canary(uint64_t *canary_ptr) {
    *canary_ptr = generate_canary();
    /* Ensure the value is visible before any function uses it */
    asm volatile("" ::: "memory");
}

/* Walk page tables and clear the PRESENT bit for virtual page 'addr'.
   pml4 must be a pointer to the top-level page table (physical mapping).
   Caller must ensure page tables are mapped and writable. */
bool set_guard_page(uint64_t *pml4, void *addr) {
    uintptr_t va = (uintptr_t)addr;
    const int shifts[4] = {39, 30, 21, 12};
    uint64_t *table = pml4;
    for (int level = 0; level < 4; ++level) {
        size_t idx = (va >> shifts[level]) & 0x1FF;
        uint64_t ent = table[idx];
        if (!(ent & PTE_PRESENT)) return false; /* already not present */
        if (level == 3) {
            /* Clear present bit in leaf PTE */
            table[idx] = ent & ~PTE_PRESENT;
            invlpg((void*)(va & ~(PAGE_SIZE-1)));
            return true;
        }
        /* Follow next-level physical address (bits 12..51) */
        uint64_t next_pa = ent & 0x000FFFFFFFFFF000ULL;
        table = (uint64_t*)(uintptr_t)next_pa; /* assumes identity mapping or caller mapped PA->VA */
    }
    return false;
}

/* Simple runtime check: panic if canary mismatches */
void check_stack_canary(uint64_t *canary_ptr, uint64_t expected) {
    if (*canary_ptr != expected) {
        /* Replace with platform-specific panic (halt, dump state, reset) */
        asm volatile("cli; hlt" ::: "memory");
    }
}