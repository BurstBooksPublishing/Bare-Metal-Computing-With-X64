#include <stdint.h>

#define TSS_GDT_INDEX 5     /* GDT index where 16-byte TSS descriptor was placed */
#define TSS_SELECTOR (TSS_GDT_INDEX * 8) /* RPL=0 */

struct __attribute__((packed)) tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};

static struct tss_entry tss __attribute__((aligned(16)));

void tss_init(void *kernel_stack_top, void *ist1_top)
{
    /* Zero entire TSS first to ensure reserved fields are cleared */
    for (size_t i = 0; i < sizeof(tss); ++i) ((uint8_t*)&tss)[i] = 0;

    tss.rsp0 = (uint64_t)kernel_stack_top; /* kernel stack used on CPL3->CPL0 transitions */
    tss.ist1 = (uint64_t)ist1_top;         /* emergency stack for IST index 1 */

    tss.iomap_base = sizeof(tss); /* disable I/O bitmap by placing it after the TSS */

    /* Load TR with the selector that points to the TSS descriptor in the GDT.
       LTR takes a 16-bit selector in %ax; move selector into %ax before LTR. */
    uint16_t sel = (uint16_t)TSS_SELECTOR;
    asm volatile ("ltr %ax" :: "a"(sel) : "memory");
}