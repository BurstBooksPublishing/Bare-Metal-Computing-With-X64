#include 

/* Task control block: only the saved stack pointer is required if
   each task's stack contains the interrupted register frame. */
typedef struct task_t {
    uint64_t *rsp;     // pointer to saved register frame (task stack top)
    // additional fields: id, state, priority, xstate pointer, etc.
} task_t;

/* Atomic swap of stack pointers: save current RSP into old, load new RSP.
   Execute with interrupts disabled (caller responsibility). */
static inline void context_switch(task_t *old, task_t *next) {
    asm volatile (
        "mov %%rsp, (%0)\n\t"   /* save RSP into old->rsp */
        "mov (%1), %%rsp\n\t"   /* load RSP from next->rsp */
        : /* no outputs */
        : "r"( &old->rsp ), "r"( &next->rsp )
        : "memory"
    );
}

/* Example scheduler tick handler called from a low-level interrupt prologue.
   The prologue must have pushed a full general-register frame and EIP/CS/RFLAGS.
   This handler runs with interrupts masked or on an IST stack. */
void timer_tick_handler(void) {
    /* 1) Minimal locking: interrupts are already disabled by IRQ entry. */

    /* 2) Choose next task (round-robin example). */
    extern task_t *current_task, *pick_next_task(void);
    task_t *next = pick_next_task();

    if (next != current_task) {
        /* 3) Switch stacks: this stores the interrupted context (frame on stack)
              into current_task->rsp and loads the next task's frame. */
        context_switch(current_task, next);

        /* 4) Update current pointer; after returning from this function,
              the interrupt epilogue will IRETQ into the next task. */
        current_task = next;
    }

    /* 5) Return to interrupt epilogue; IRETQ will restore RIP/CS/RFLAGS and
          the saved register frame of the resumed task. */
}