// Read/write MSR (IA32_EFER = 0xC0000080).
static inline uint64_t rdmsr(uint32_t msr){
    uint32_t lo,hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static inline void wrmsr(uint32_t msr, uint64_t val){
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

// Read/Write CR4
static inline uint64_t readcr4(void){
    uint64_t v;
    __asm__ volatile("mov %%cr4, %0" : "=r"(v));
    return v;
}
static inline void writecr4(uint64_t v){
    __asm__ volatile("mov %0, %%cr4" :: "r"(v));
}

// WRPKRU: write protection key rights; EAX=pkru, ECX=0, EDX=0.
static inline void writepkru(uint32_t pkru){
    __asm__ volatile(".byte 0x0f,0x01,0xef" /*wrpkru*/ : : "a"(pkru), "c"(0), "d"(0));
}

void enable_security_primitives(void){
    // Enable SMEP (bit20), SMAP (bit21), PKE (bit22) in CR4.
    const uint64_t CR4_SMEP = 1ULL << 20;
    const uint64_t CR4_SMAP = 1ULL << 21;
    const uint64_t CR4_PKE  = 1ULL << 22;
    uint64_t cr4 = readcr4();
    cr4 |= (CR4_SMEP | CR4_SMAP | CR4_PKE);
    writecr4(cr4);

    // Enable NXE in  (MSR 0xC0000080, bit 11).
    const uint32_t MSR_EFER = 0xC0000080;
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= (1ULL << 11);
    wrmsr(MSR_EFER, efer);

    // Set PKRU: deny writes to pkey0 (bits 2:0 per pkey; 2 bits per key: AD,WD).
    // For pkey0: set WD=1 to disable writes. Layout: per-key 2 bits; key i at bits 2*i..2*i+1
    uint32_t pkru = (1u << 1); // set WD for key0
    writepkru(pkru);
}