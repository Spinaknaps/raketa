#ifndef RAKETA_TIMER_H
#define RAKETA_TIMER_H

#include <stdint.h>

void timer_init(void);
uint32_t timer_poll_ms(void);

#endif