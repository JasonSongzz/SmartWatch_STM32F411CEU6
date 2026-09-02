#ifndef __DELAY_H
#define __DELAY_H

#include <stdint.h>

/* Initialize the cycle counter used by delay_us(). Call after SystemClock_Config(). */
void delay_init(void);

/* Busy-wait delay for software timing. Safe before the scheduler starts. */
void delay_us(uint32_t us);

/* Task-aware millisecond delay. Uses osDelay() from a running task, HAL_Delay() otherwise. */
void delay_ms(uint32_t ms);

#endif /* __DELAY_H */
