#include 
#include 

/* Read 32-bit MMIO register (volatile pointer, compiler barrier). */
static inline uint32_t mmio_read32(uintptr_t addr)
{
    volatile uint32_t *p = (volatile uint32_t *)addr;
    uint32_t v = *p;                 /* device read (may have side effect) */
    asm volatile("" ::: "memory");   /* compiler fence */
    return v;
}

/* Write 32-bit MMIO register and return a read-back to ensure completion. */
static inline void mmio_write32_flush(uintptr_t addr, uint32_t val)
{
    volatile uint32_t *p = (volatile uint32_t *)addr;
    *p = val;                        /* write may be posted */
    asm volatile("" ::: "memory");   /* compiler fence */
    (void)*p;                        /* read-back forces completion on PCIe */
    asm volatile("" ::: "memory");
}

/* Example: publish a new producer index then ring doorbell.
   Parameters:
     descs_addr: physical/identity-mapped descriptor base already updated
     prod_index: new producer index to publish
     doorbell_mmio: MMIO address of device doorbell register
*/
void publish_and_ring(uint32_t prod_index, uintptr_t doorbell_mmio)
{
    /* Ensure prior descriptor stores are visible before we write doorbell.
       A store fence (sfence) may be used, but read-back is required for PCIe. */
    asm volatile("sfence" ::: "memory");         /* ensure descriptor stores reach WC buffers */
    mmio_write32_flush(doorbell_mmio, prod_index); /* ring doorbell and flush */
}