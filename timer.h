#ifndef _TIMER_H_
#define _TIMER_H_

#include <util.h>

PUBLIC bool initMSTimer(void);
PUBLIC uint64_t timer_read_counter(void);
PUBLIC void delay_ms(uint64_t ms_count);

#endif