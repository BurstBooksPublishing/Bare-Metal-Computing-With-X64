#include 
#include 

#define STACK_POOL_BYTES (1<<20) /* 1 MiB pool for stacks */
#define PAGE_SIZE 4096

static uint8_t stack_pool[STACK_POOL_BYTES] __attribute__((aligned(PAGE_SIZE)));
static size_t stack_pool_offset = 0;

/* Allocate a stack region with a single guard page. Returns stack-top pointer. */
void *allocate_stack(size_t usable_size, void **out_guard_page) {
    /* round usable_size up to page granularity */
    size_t total = ((usable_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    /* account for one guard page */
    size_t alloc = total + PAGE_SIZE;
    if (stack_pool_offset + alloc > STACK_POOL_BYTES) return NULL;
    uint8_t *base = &stack_pool[stack_pool_offset];
    stack_pool_offset += alloc;
    /* layout: [guard page (low)] [usable stack (high)] */
    void *guard = (void *)base;
    void *stack_top = (void *)(base + alloc); /* top is one past usable region */
    /* ensure 16-byte alignment of returned stack top */
    uintptr_t st = (uintptr_t)stack_top;
    st &= ~((uintptr_t)0xF);
    stack_top = (void *)st;
    if (out_guard_page) *out_guard_page = guard;
    /* Caller must map/unmap the guard page in page tables; here we leave it reserved. */
    return stack_top;
}

/* Switch stacks: save current RSP into *old_sp, load new_sp into RSP. */
static inline void switch_stack(void **old_sp, void *new_sp) {
    __asm__ volatile (
        "movq %%rsp, (%0)\n\t" /* save old RSP */
        "movq %1, %%rsp\n\t"   /* set new RSP */
        :
        : "r"(old_sp), "r"(new_sp)
        : "memory"
    );
}