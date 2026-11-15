#include "timer.h"

#define REG32(address) (*(volatile uint32_t *)(address))

#define SYSTICK_CTRL REG32(0xE000E010u)
#define SYSTICK_LOAD REG32(0xE000E014u)
#define SYSTICK_VAL  REG32(0xE000E018u)

#define SYSTICK_ENABLE    (1u << 0)
#define SYSTICK_CPU_CLOCK (1u << 2)
#define SYSTICK_COUNTFLAG (1u << 16)

#define CPU_CLOCK_HZ 12000000u
#define TICKS_PER_MS (CPU_CLOCK_HZ / 1000u)

static uint32_t elapsed_ms;

void timer_init(void)
{
    elapsed_ms = 0;
    SYSTICK_CTRL = 0;
    SYSTICK_LOAD = TICKS_PER_MS - 1u;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = SYSTICK_CPU_CLOCK | SYSTICK_ENABLE;
}

uint32_t timer_poll_ms(void)
{
    if ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) != 0u) {
        ++elapsed_ms;
    }

    return elapsed_ms;
}
