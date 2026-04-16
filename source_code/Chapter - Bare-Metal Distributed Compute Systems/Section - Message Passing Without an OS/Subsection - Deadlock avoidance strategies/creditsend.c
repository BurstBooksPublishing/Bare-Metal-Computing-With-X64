#include 
#include 
#include 

// Shared, memory-mapped credit counter provided by receiver.
atomic_int *recv_credits;      // pointer to remote credit count
uint8_t *tx_ring;              // pointer to transmit descriptors
size_t tx_ring_size;           // power-of-two

// Write memory barrier for x64; C11 fence used for portability.
static inline void write_barrier(void) { atomic_thread_fence(memory_order_release); }
static inline void read_barrier(void)  { atomic_thread_fence(memory_order_acquire); }

// Nonblocking attempt to send a single descriptor.
// Returns 0 on success, -1 if no credits.
int try_send(const void *desc, size_t desc_len)
{
    int credits = atomic_load_explicit(recv_credits, memory_order_acquire);
    if (credits <= 0) return -1;                    // no credit, avoid blocking

    // attempt to decrement remote credit atomically (optimistic).
    // This uses fetch_sub; on bare-metal NICs, translate to appropriate MMIO/CAS.
    int prev = atomic_fetch_sub_explicit(recv_credits, 1, memory_order_acq_rel);
    if (prev <= 0) {
        // lost race; restore and fail
        atomic_fetch_add_explicit(recv_credits, 1, memory_order_release);
        return -1;
    }

    // Enqueue descriptor into local ring (lock-free single-producer example).
    static atomic_size_t tx_tail = 0;
    size_t idx = atomic_fetch_add_explicit(&tx_tail, 1, memory_order_acq_rel) & (tx_ring_size - 1);
    // copy descriptor into ring slot
    uint8_t *slot = tx_ring + idx * desc_len;
    memcpy(slot, desc, desc_len);

    // ensure descriptor is visible before notifying NIC
    write_barrier();
    // notify NIC (MMIO doorbell, omitted; platform-specific)
    // mmio_write(NIC_DOORBELL, idx);
    return 0;
}