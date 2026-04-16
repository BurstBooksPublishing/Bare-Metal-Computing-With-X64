#include 
#include 

// E820-like entry: base, length, type==1 means usable RAM.
typedef struct { uint64_t base; uint64_t length; uint32_t type; } e820_entry_t;

// Find highest usable RAM below 1MB and compute SS:SP for a stack top.
// margin_bytes: safety margin to leave above the stack (e.g., 512).
// Returns 0 on success, non-zero on failure.
int choose_real_mode_stack(const e820_entry_t *map, size_t entries,
                           uint32_t margin_bytes,
                           uint16_t *out_SS, uint16_t *out_SP)
{
    const uint64_t limit = 0x100000ULL; // 1MB physical limit for real mode
    uint64_t best_end = 0;
    uint64_t best_base = 0;

    // Pick the highest usable region whose end is below the 1MB limit.
    for (size_t i = 0; i < entries; ++i) {
        if (map[i].type != 1) continue;                  // skip non-usable
        uint64_t region_base = map[i].base;
        uint64_t region_end = map[i].base + map[i].length;
        if (region_base >= limit) continue;             // entirely above 1MB
        if (region_end > limit) region_end = limit;     // clamp to 1MB
        if (region_end > best_end) { best_end = region_end; best_base = region_base; }
    }

    if (best_end == 0 || best_end - best_base <= margin_bytes) return -1;

    uint64_t stack_top = best_end - margin_bytes;      // P_stack from text
    // Compute SS = floor(stack_top/16), SP = stack_top mod 16.
    *out_SS = (uint16_t)(stack_top >> 4);
    *out_SP = (uint16_t)(stack_top & 0xF);
    return 0;
}