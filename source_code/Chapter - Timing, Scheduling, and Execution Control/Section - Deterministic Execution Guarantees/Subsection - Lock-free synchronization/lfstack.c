#include 
#include 
#include 

/* Node layout suitable for bare-metal allocation. */
typedef struct node { struct node *next; void *data; } node_t;

/* Top is 16-byte aligned; ptr plus 64-bit tag. */
typedef struct {
    alignas(16) volatile uint64_t ptr;
    volatile uint64_t tag;
} top_t;

typedef struct { top_t top; } lfstack_t;

/* cmpxchg16b: returns 1 on success, 0 on failure. */
static inline int cas128(volatile void *addr,
                         uint64_t expected_lo, uint64_t expected_hi,
                         uint64_t new_lo, uint64_t new_hi)
{
    unsigned char ok;
    /* Uses rax=rlo, rdx=rhi as expected; rbx=rlo_new, rcx=rhi_new as new. */
    asm volatile(
        "lock cmpxchg16b %0\n\t"
        "setz %1"
        : "+m" (*(volatile unsigned __int128 *)addr), "=q" (ok)
        : "a" (expected_lo), "d" (expected_hi), "b" (new_lo), "c" (new_hi)
        : "cc", "memory"
    );
    return ok;
}

void lfstack_push(lfstack_t *s, node_t *n)
{
    for (;;) {
        uint64_t old_ptr = (uint64_t) s->top.ptr;
        uint64_t old_tag = s->top.tag;
        n->next = (node_t *) old_ptr;
        uint64_t new_ptr = (uint64_t) n;
        uint64_t new_tag = old_tag + 1;
        if (cas128(&s->top, old_ptr, old_tag, new_ptr, new_tag)) return;
        /* Backoff or PAUSE can be inserted here to reduce contention. */
    }
}

node_t *lfstack_pop(lfstack_t *s)
{
    for (;;) {
        uint64_t old_ptr = (uint64_t) s->top.ptr;
        uint64_t old_tag = s->top.tag;
        if (old_ptr == 0) return NULL;
        node_t *n = (node_t *) old_ptr;
        uint64_t next_ptr = (uint64_t) n->next;
        uint64_t new_tag = old_tag + 1;
        if (cas128(&s->top, old_ptr, old_tag, next_ptr, new_tag)) return n;
    }
}