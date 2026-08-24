#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_install(void);
uint32_t timer_get_ticks(void);

#endif