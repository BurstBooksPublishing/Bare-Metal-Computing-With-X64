#include 
#include 
#include 

// Calibration: set tsc_hz at init (cycles per second).
extern uint64_t tsc_hz;                // calibrated invariant TSC frequency

static inline uint64_t rdtsc64(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline double tsc_to_sec(uint64_t cycles){ return (double)cycles / (double)tsc_hz; }

typedef struct {
    double ewma_mu;     // EWMA estimate of inter-arrival mean (seconds)
    uint64_t last_tsc;  // last heartbeat timestamp (cycles)
    bool initialized;
} peer_fd_t;

#define ALPHA 0.2
#define PHI_THRESHOLD 8.0
#define CONFIRM_PERIOD_SEC 0.05

// Called when a heartbeat packet from peer_id is observed; ts is rdtsc at receipt.
void on_heartbeat(peer_fd_t *p, uint64_t ts){
    if(!p->initialized){
        p->last_tsc = ts; p->initialized = true;
        p->ewma_mu = 0.0; return;
    }
    uint64_t dcycles = ts - p->last_tsc;
    double dt = tsc_to_sec(dcycles);
    if(p->ewma_mu <= 0.0) p->ewma_mu = dt;
    else p->ewma_mu = ALPHA * dt + (1.0 - ALPHA) * p->ewma_mu;
    p->last_tsc = ts;
}

// Compute phi assuming exponential inter-arrival with lambda = 1/mu.
double compute_phi(const peer_fd_t *p, uint64_t now_tsc){
    if(!p->initialized) return INFINITY;
    double dt = tsc_to_sec(now_tsc - p->last_tsc);
    double lambda = 1.0 / p->ewma_mu;              // safe when ewma_mu>0
    return (lambda * dt) / log(10.0);              // from Eq. (1)
}

// Main poll loop: nic_poll populates src_id and timestamps with rdtsc at RX.
void detector_loop(peer_fd_t peers[], size_t n_peers){
    uint64_t now;
    while(true){
        // Poll RX; nic_receive returns true if a heartbeat packet arrived.
        uint8_t src; uint64_t rx_tsc;
        if(nic_receive(&src, &rx_tsc)){            // platform-specific RX polling
            on_heartbeat(&peers[src], rx_tsc);
        }
        now = rdtsc64();
        for(size_t i=0;i= PHI_THRESHOLD){
                // transient check: require confirmation window to avoid flapping
                double suspect_for = tsc_to_sec(now - peers[i].last_tsc);
                if(suspect_for >= CONFIRM_PERIOD_SEC){
                    mark_suspected(i);            // application-defined action
                }
            }
        }
        // optionally relax loop with pause or APIC timer sleep
        __asm__ volatile("pause");
    }
}