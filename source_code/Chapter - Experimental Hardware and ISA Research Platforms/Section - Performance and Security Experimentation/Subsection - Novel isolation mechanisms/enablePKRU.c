#include 

// Set CR4.PKE (bit 22) to enable Protection Keys (must run in ring 0).
static inline void enable_pke_cr4(void) {
    unsigned long cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));            // read CR4
    cr4 |= (1UL << 22);                                  // set PKE bit
    asm volatile("mov %0, %%cr4" :: "r"(cr4) : "memory"); // write CR4
}

// Write PKRU register; pkru: 32-bit value where each key has AD/WD bits.
// Use "wrpkru" with EAX=pkru, ECX=0, EDX=0. Caller must ensure CPU supports PKU.
static inline void write_pkru(uint32_t pkru) {
    asm volatile("lfence\n\t"                              // serialize previous loads
                 "wrpkru\n\t"                               // update PKRU
                 "lfence"                                   // serialize following loads
                 :: "a"(pkru), "c"(0), "d"(0) : "memory");
}

// Example: disable read/write for protection key 3 (set AD and WD).
static inline uint32_t pkru_disable_key(unsigned key) {
    return (1u << (2*key)) | (1u << (2*key + 1)); // AD and WD bits
}

// Usage in bare-metal initialization:
void setup_pku_isolation(void) {
    enable_pke_cr4();                     // must be privileged
    uint32_t pkru = pkru_disable_key(3);  // revoke accesses to key 3
    write_pkru(pkru);                     // atomically program PKRU
    // Next: ensure pages for domain are mapped with protection key 3.
}