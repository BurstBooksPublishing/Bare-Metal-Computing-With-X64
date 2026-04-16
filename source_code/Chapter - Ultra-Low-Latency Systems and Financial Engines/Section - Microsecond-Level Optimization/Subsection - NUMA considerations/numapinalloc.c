/* Compile: gcc -O2 -lnuma -pthread -o numa_test numa_test.c */
/* Purpose: pin a thread to cpu and allocate 2MB hugepage buffer on a NUMA node */
#define _GNU_SOURCE
#include 
#include 
#include 
#include 
#include 
#include 
#include 

static void die(const char *s){ perror(s); exit(EXIT_FAILURE); }

/* worker: pins itself to 'cpu' and uses a buffer from node 'node' */
void *worker(void *arg){
    int cpu = ((int*)arg)[0];
    int node = ((int*)arg)[1];
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(cpu, &cpus);
    if (sched_setaffinity(0, sizeof(cpus), &cpus) != 0) die("sched_setaffinity");

    /* allocate 2MB using libnuma on the requested node */
    size_t sz = 2 * 1024 * 1024;
    void *buf = numa_alloc_onnode(sz, node);
    if (!buf) die("numa_alloc_onnode");
    /* touch to fault pages in */
    memset(buf, 0, sz);

    /* application-specific low-latency loop; keep simple for demo */
    volatile uint64_t sum = 0;
    for (size_t i = 0; i < sz/8; i += 64) sum += ((uint64_t*)buf)[i];

    numa_free(buf, sz);
    return (void*)(uintptr_t)sum;
}

int main(int argc, char **argv){
    if (numa_available() < 0) die("NUMA not available");
    int cpu = 2; /* choose CPU on node 0 in test deployment */
    int node = numa_node_of_cpu(cpu);
    if (node < 0) die("numa_node_of_cpu");

    int args[2] = { cpu, node };
    pthread_t t;
    if (pthread_create(&t, NULL, worker, args) != 0) die("pthread_create");
    void *res;
    if (pthread_join(t, &res) != 0) die("pthread_join");
    printf("worker returned %p\n", res);
    return 0;
}