#include 
#include 

/* PTE bit definitions for x86-64 */
#define PTE_PRESENT  (1ULL<<0)
#define PTE_WRITABLE (1ULL<<1)

/* Atomically clear present bit of a PTE and invalidate the page.
   pte_ptr must point to the 64-bit page-table entry controlling va. */
static inline void arm_guard_page(uint64_t *pte_ptr, void *va) {
    uint64_t old = __atomic_fetch_and(pte_ptr, ~PTE_PRESENT, __ATOMIC_SEQ_CST);
    (void)old; // old used if logging required
    // Invalidate the single page from TLB so CR3 does not still allow access.
    asm volatile("invlpg (%0)" : : "r"(va) : "memory");
}

/* Example page-fault handler excerpt (called with faulting RIP and error_code)
   CR2 contains the faulting linear address per x86 semantics. */
void page_fault_handler(uint64_t rip, uint64_t error_code) {
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    // Check whether fault_addr lies inside a known guard region.
    if (is_guard_address((void*)fault_addr)) {
        // Report structured crash and halt or attempt safe unwind.
        structured_report_guard_hit((void*)fault_addr, rip, error_code);
    } else {
        // Handle normal page-fault or escalate to triple-fault avoidance path.
        handle_normal_pf((void*)fault_addr, rip, error_code);
    }
}