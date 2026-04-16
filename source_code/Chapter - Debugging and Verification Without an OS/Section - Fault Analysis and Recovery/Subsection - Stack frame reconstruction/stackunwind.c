#include 
#include 
#include 

// Validate canonical 64-bit virtual address.
static inline bool is_canonical(uint64_t a){
    return ((a >> 47) == 0) || ((a >> 47) == ((1ULL<<17)-1));
}

// Conservative unwinder: walk RBP-linked frames and collect return RIPs.
// rbp: initial frame pointer from fault context.
// text_start/text_end: executable region bounds to validate RIPs.
// out_rips: caller-provided buffer; returns count written.
// Notes: caller must ensure target memory is mapped and readable.
size_t unwind_frames(uint64_t rbp, uint64_t text_start, uint64_t text_end,
                     uint64_t *out_rips, size_t max_frames){
    size_t n = 0;
    for(size_t depth = 0; depth < max_frames; ++depth){
        if(!is_canonical(rbp)) break;            // invalid pointer
        if(rbp & 0x7) break;                    // misaligned
        uint64_t *p = (uint64_t*) (uintptr_t) rbp;
        // Defensive reads: in a real handler, bound-check or use safe_read.
        uint64_t saved_rbp = p[0];
        uint64_t ret_rip   = p[1];
        if(!is_canonical(saved_rbp)) break;
        if(ret_rip < text_start || ret_rip >= text_end) break;
        out_rips[n++] = ret_rip;
        if(saved_rbp == 0 || saved_rbp <= rbp) break; // terminator or loop
        rbp = saved_rbp;
    }
    return n;
}