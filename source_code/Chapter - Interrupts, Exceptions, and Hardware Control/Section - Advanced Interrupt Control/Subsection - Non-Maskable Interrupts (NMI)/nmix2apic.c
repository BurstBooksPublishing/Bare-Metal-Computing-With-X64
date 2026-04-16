#include 
static inline void wrmsr(uint32_t msr, uint64_t val){
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    asm volatile("wrmsr" :: "c"(msr), "a"(low), "d"(high) : "memory");
}
/* Send NMI to a target APIC id using x2APIC MSR 0x830 */
void send_x2apic_nmi(uint32_t apic_id){
    uint64_t icr = 0;
    /* delivery mode NMI = 0b100 in bits 8..10; vector 0 ignored. */
    icr |= ((uint64_t)0b100 << 8);
    /* set destination APIC id in bits 32..63 */
    icr |= ((uint64_t)apic_id << 32);
    wrmsr(0x830, icr);
}