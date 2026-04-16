/* Simple linear-scan allocator for x64 temporaries. */
#include 
#include 

typedef struct {
    int vreg;      /* virtual register id */
    int start;     /* inclusive instruction index */
    int end;       /* exclusive instruction index */
    int assigned;  /* physical reg index or -1 for spilled */
    int spill_slot;/* stack slot index if spilled, -1 if none */
} Interval;

/* Example physical registers available for allocation. */
static const char *phys_regs[] = {
    "rax","rcx","rdx","rsi","rdi","r8","r9","r10","r11"
};
#define P (sizeof(phys_regs)/sizeof(phys_regs[0]))

/* Compare by start, then by end. */
static int cmp_start(const void *a, const void *b){
    const Interval *A = a, *B = b;
    if (A->start != B->start) return A->start - B->start;
    return A->end - B->end;
}

/* Expire intervals that have ended before curr->start. */
static void expire_old(Interval *active[], int *active_cnt, Interval *curr){
    for (int i = 0; i < *active_cnt; ){
        if (active[i]->end <= curr->start){
            /* remove active[i] */
            active[i] = active[--(*active_cnt)];
        } else i++;
    }
}

/* Find active interval with largest end; return its index */
static int find_longest(Interval *active[], int active_cnt){
    int idx = 0;
    for (int i = 1; i < active_cnt; ++i)
        if (active[i]->end > active[idx]->end) idx = i;
    return idx;
}

/* Main allocator: intervals array of length n. */
void linear_scan_allocate(Interval intervals[], int n){
    qsort(intervals, n, sizeof(Interval), cmp_start);
    Interval *active[P]; int active_cnt = 0;
    int reg_used[P]; for (int i=0;iassigned = free_reg; cur->spill_slot = -1;
            reg_used[free_reg] = 1; active[active_cnt++] = cur;
        } else {
            int victim_idx = find_longest(active, active_cnt);
            Interval *victim = active[victim_idx];
            if (victim->end > cur->end){
                /* spill victim */
                cur->assigned = victim->assigned;
                cur->spill_slot = -1;
                victim->assigned = -1;
                victim->spill_slot = next_spill++; /* assign stack slot */
                active[victim_idx] = cur;
            } else {
                /* spill current interval */
                cur->assigned = -1;
                cur->spill_slot = next_spill++;
            }
        }
    }
    /* Print allocation mapping for inspection. */
    for (int i=0;i= 0)
            printf("v%d -> %s (live [%d,%d))\n",
                intervals[i].vreg, phys_regs[intervals[i].assigned],
                intervals[i].start, intervals[i].end);
        else
            printf("v%d -> spill@slot%d (live [%d,%d))\n",
                intervals[i].vreg, intervals[i].spill_slot,
                intervals[i].start, intervals[i].end);
    }
}