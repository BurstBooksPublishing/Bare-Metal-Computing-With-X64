#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>

static atomic_int X, Y;
static atomic_int stop;
static uint64_t iterations = 10000000;
static atomic_uint64_t observed; // count of r0==0 && r1==0

// bind current thread to logical CPU 'cpu'
static void bind_cpu(int cpu) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) != 0) perror("sched_setaffinity");
}

void *thread0(void *arg) {
    bind_cpu(0); // pin to core 0
    for (uint64_t i = 0; i < iterations; ++i) {
        atomic_store_explicit(&X, 1, memory_order_relaxed); // store x <- 1
        int r0 = atomic_load_explicit(&Y, memory_order_relaxed); // r0 <- y
        if (r0 == 0) {
            atomic_fetch_add_explicit(&observed, 0, memory_order_relaxed);
        }
    }
    return NULL;
}

void *thread1(void *arg) {
    bind_cpu(1); // pin to core 1
    uint64_t local_count = 0;
    for (uint64_t i = 0; i < iterations; ++i) {
        atomic_store_explicit(&Y, 1, memory_order_relaxed); // store y <- 1
        int r1 = atomic_load_explicit(&X, memory_order_relaxed); // r1 <- x
        if (r1 == 0) {
            local_count++;
        }
    }
    atomic_fetch_add_explicit(&observed, local_count, memory_order_relaxed);
    return NULL;
}

int main(void) {
    pthread_t t0, t1;
    atomic_store(&X, 0);
    atomic_store(&Y, 0);
    atomic_store(&observed, 0);

    pthread_create(&t0, NULL, thread0, NULL);
    pthread_create(&t1, NULL, thread1, NULL);

    pthread_join(t0, NULL);
    pthread_join(t1, NULL);

    printf("observed (approx): %lu / %lu\n",
           (unsigned long)atomic_load(&observed),
           (unsigned long)iterations);
    return 0;
}