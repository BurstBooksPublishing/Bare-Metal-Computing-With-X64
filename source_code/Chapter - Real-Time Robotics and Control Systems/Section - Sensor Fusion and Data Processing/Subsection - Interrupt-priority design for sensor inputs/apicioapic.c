#include 
#include 
/* APIC and IOAPIC bases must be discovered from ACPI MADT or MSR 0x1B. */
volatile uint32_t *local_apic_base;   // mapped to 0xFEE00000 or discovered base
volatile uint32_t *ioapic_base;       // mapped to 0xFEC00000 or discovered base

static inline void apic_write(uint32_t offset, uint32_t value){
    local_apic_base[offset/4] = value;
    (void)local_apic_base[offset/4]; // read-back for posted write ordering
}

static inline uint32_t apic_read(uint32_t offset){
    return local_apic_base[offset/4];
}

/* Set Task Priority Register (TPR): only high 4 bits are compared against vector priority. */
void set_tpr(uint8_t tpr_priority_byte){
    apic_write(0x80, (uint32_t)tpr_priority_byte);
}

/* IOAPIC register writes: index at +0x00, data at +0x10 (standard map). */
static inline void ioapic_write(uint8_t reg, uint32_t value){
    volatile uint32_t *io_regsel = (uint32_t*)((uintptr_t)ioapic_base + 0x00);
    volatile uint32_t *io_iowin  = (uint32_t*)((uintptr_t)ioapic_base + 0x10);
    *io_regsel = reg;
    *io_iowin  = value;
}

/* Route a given IOAPIC redirection entry to vector with given polarity and trigger mode.
   redir_index: 2*N low dword index (upper dword is redir_index+1). */
void route_ioapic_entry(uint8_t redir_index, uint8_t vector, uint8_t dest_apic_id, 
                        int active_low, int level_trigger){
    uint32_t low = vector; /* bits 0-7: vector */
    if(active_low) low |= (1u<<13);
    if(level_trigger) low |= (1u<<15);
    uint32_t high = ((uint32_t)dest_apic_id) << 24;
    ioapic_write(redir_index, low);
    ioapic_write(redir_index+1, high);
}