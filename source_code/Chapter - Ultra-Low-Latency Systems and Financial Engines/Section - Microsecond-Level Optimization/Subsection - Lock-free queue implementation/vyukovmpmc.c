/* Minimal, production-ready bounded MPMC queue. Compile with -std=c11. */
#include 
#include 
#include 

#define CACHELINE 64

typedef struct {
    atomic_size_t seq;      // sequence number for this cell
    void *data;             // stored pointer payload
    char pad[CACHELINE - sizeof(atomic_size_t) - sizeof(void*)];
} cell_t;

typedef struct {
    size_t capacity;
    size_t mask;
    cell_t *buffer;                   // malloc aligned to CACHELINE
    alignas(CACHELINE) atomic_size_t head;
    alignas(CACHELINE) atomic_size_t tail;
} mpmc_ring_t;

/* compiler/hint helpers */
static inline void cpu_relax(void) { asm volatile("pause" ::: "memory"); }

/* Initialize: capacity must be power-of-two. buffer must be capacity-aligned. */
int mpmc_init(mpmc_ring_t *q, cell_t *buf, size_t capacity) {
    if ((capacity & (capacity - 1)) != 0) return -1;
    q->capacity = capacity;
    q->mask = capacity - 1;
    q->buffer = buf;
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    for (size_t i = 0; i < capacity; ++i) {
        atomic_init(&buf[i].seq, i);
        buf[i].data = NULL;
    }
    return 0;
}

/* Enqueue returns 0 on success, -1 if full (non-blocking). */
int mpmc_enqueue(mpmc_ring_t *q, void *ptr) {
    size_t pos = atomic_fetch_add_explicit(&q->tail, 1, memory_order_relaxed);
    cell_t *cell = &q->buffer[pos & q->mask];

    for (;;) {
        size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)pos;
        if (dif == 0) {
            /* Slot is ours. Publish payload and mark ready. */
            cell->data = ptr;
            atomic_store_explicit(&cell->seq, pos + 1, memory_order_release);
            return 0;
        } else if (dif < 0) {
            /* seq < pos => buffer full (producers outrun consumers). */
            return -1;
        } else {
            cpu_relax();
        }
    }
}

/* Dequeue returns 0 on success, -1 if empty (non-blocking). */
int mpmc_dequeue(mpmc_ring_t *q, void **out) {
    size_t pos = atomic_fetch_add_explicit(&q->head, 1, memory_order_relaxed);
    cell_t *cell = &q->buffer[pos & q->mask];

    for (;;) {
        size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
        if (dif == 0) {
            /* Slot contains valid data. Consume and mark free. */
            *out = cell->data;
            atomic_store_explicit(&cell->seq, pos + q->capacity, memory_order_release);
            return 0;
        } else if (dif < 0) {
            /* seq < pos+1 => buffer empty (consumers outrun producers). */
            return -1;
        } else {
            cpu_relax();
        }
    }
}