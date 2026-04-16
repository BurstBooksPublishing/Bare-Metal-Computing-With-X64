#include 

// Interrupt frame provided by assembly stub (RIP, CS, RFLAGS, RSP, SS if CPL changed).
struct int_frame { uint64_t rip, cs, rflags, rsp, ss; };

// Read CR2 (faulting linear address).
static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

// Prototypes provided by platform-specific code.
void *alloc_phys_page(void);                // allocate one 4KiB physical frame
void map_page(uint64_t virt, uint64_t phys, uint64_t flags); // update page tables atomically
void dump_registers(const struct int_frame *f); // diagnostic output
__attribute__((noreturn)) void system_halt(void); // stop on unrecoverable faults

// Flags: present=1, rw=2, user=4 etc., platform-defined.
#define PTE_PRESENT 0x1
#define PTE_RW      0x2
#define PTE_USER    0x4

// Called by assembly stub with error_code and pointer to frame+saved registers.
void page_fault_handler(uint64_t err_code, struct int_frame *frame) {
    uint64_t fault_addr = read_cr2();
    // If non-present page, attempt on-demand mapping.
    if ((err_code & 0x1) == 0) { // P bit == 0
        void *phys = alloc_phys_page();
        if (!phys) goto fail;
        uint64_t paddr = (uint64_t)phys;
        uint64_t va = fault_addr & ~0xFFFULL; // page-align
        uint64_t flags = PTE_PRESENT | PTE_RW | PTE_USER; // conservative choice
        map_page(va, paddr, flags); // platform-specific atomic PTE install
        __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory"); // invalidate TLB
        return; // resume faulting instruction
    } else {
        // Protection violation: log context and stop deterministically.
        dump_registers(frame);
    }
fail:
    // Provide a concise crash report: vector, CR2, error.
    // (Real implementations should emit to serial or persistent log.)
    // Example logging omitted for brevity.
    system_halt();
}