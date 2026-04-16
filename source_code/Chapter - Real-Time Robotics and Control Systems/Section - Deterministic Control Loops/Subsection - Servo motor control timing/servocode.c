#include 
#define GPIO_BASE ((volatile uint32_t*)0xF00F0000) /* MMIO GPIO base */
#define GPIO_SET   0x1  /* bit mask for servo pin */
static inline uint64_t rdtscp(void){
    uint32_t aux; uint64_t r;
    asm volatile("rdtscp" : "=a"(r), "=d"(((uint32_t*)&r)[1]), "=c"(aux) ::);
    return r;
}
static inline void sfence_cpu(void){ asm volatile("sfence" ::: "memory"); }
void delay_until(uint64_t target){ while (rdtscp() < target) asm volatile("pause"); }
void generate_servo_pulse(uint64_t tsc_now, uint64_t cycles_high, uint64_t cycles_period){
    uint64_t t_start = tsc_now;
    *GPIO_BASE |= GPIO_SET; sfence_cpu();               /* set pin high */
    delay_until(t_start + cycles_high);                 /* busy-wait high time */
    *GPIO_BASE &= ~GPIO_SET; sfence_cpu();              /* clear pin low */
    delay_until(t_start + cycles_period);               /* wait until next period */
}
int main(void){
    uint64_t tsc_hz = 3000000000ULL;                     /* set measured TSC freq */
    uint64_t cycles_per_us = tsc_hz/1000000ULL;         /* from (3) */
    uint64_t period_us = 20000;                         /* 20 ms period */
    uint64_t servo_min_us = 1000, servo_max_us = 2000;  /* pulse range */
    uint64_t angle = 90;                                /* example angle */
    uint64_t pulse_us = servo_min_us + (angle* (servo_max_us-servo_min_us))/180;
    uint64_t cycles_high = pulse_us * cycles_per_us;
    uint64_t cycles_period = period_us * cycles_per_us;
    /* pin to core and disable other sources of jitter before the loop */
    asm volatile("cli");                                /* bare-metal: disable interrupts */
    while (1){
        uint64_t now = rdtscp();
        generate_servo_pulse(now, cycles_high, cycles_period);
        /* compute new pulse_us if angle updated externally */
    }
    return 0;
}