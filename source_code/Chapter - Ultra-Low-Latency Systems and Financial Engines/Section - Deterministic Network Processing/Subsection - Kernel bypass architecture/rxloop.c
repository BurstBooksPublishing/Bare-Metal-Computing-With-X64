#include 
#include 
#include 

#define BURST_SIZE 32

/* Per-core RX processing loop. Assumes the port and queue are already configured. */
static void rx_loop(uint16_t port_id, uint16_t queue_id) {
    struct rte_mbuf *pkts[BURST_SIZE];
    for (;;) {
        /* Poll without syscalls; returns 0..BURST_SIZE packets. */
        const uint16_t nb_rx = rte_eth_rx_burst(port_id, queue_id, pkts, BURST_SIZE);
        if (unlikely(nb_rx == 0)) {
            /* Optional pause to prevent starving other cores; use cpu_relax() */
            asm volatile("pause" ::: "memory");
            continue;
        }

        /* Prefetch packet headers and first cacheline of payload. */
        for (uint16_t i = 0; i < nb_rx; ++i) {
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
        }

        /* Fast-path packet processing: inline minimal processing for determinism. */
        for (uint16_t i = 0; i < nb_rx; ++i) {
            struct rte_mbuf *m = pkts[i];
            uint8_t *data = rte_pktmbuf_mtod(m, uint8_t *);
            const uint16_t len = rte_pktmbuf_pkt_len(m);

            /* Example: parse headers and craft minimal application decision. */
            if (likely(len >= 14)) {
                /* Process L2/L3 fields using aligned loads for speed. */
                // ... deterministic, branch-minimized processing ...
            }

            /* Hand the packet to application stage or free after zero-copy handoff. */
            process_packet(m);
            rte_pktmbuf_free(m);
        }
    }
}