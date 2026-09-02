#ifndef __OS_FREERTOS_H__
#define __OS_FREERTOS_H__

#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "cmsis_compiler.h"
#include "common_types.h"

static inline TickType_t os_freertos_ms_to_ticks(osal_time_ms_t time_ms)
{
    uint64_t ticks;

    if (time_ms == OSAL_WAIT_FOREVER) {
        return portMAX_DELAY;
    }
    if (time_ms == 0U) {
        return (TickType_t)0U;
    }
    /* Round up so a finite timeout never expires earlier than requested. */
    ticks = ((uint64_t)time_ms * (uint64_t)configTICK_RATE_HZ + 999ULL) /
            1000ULL;
    if (ticks >= (uint64_t)portMAX_DELAY) {
        ticks = (uint64_t)portMAX_DELAY - 1ULL;
    }
    return (TickType_t)ticks;
}

static inline osal_time_ms_t os_freertos_ticks_to_ms(TickType_t ticks)
{
    return (osal_time_ms_t)(((uint64_t)ticks * 1000ULL) /
                            (uint64_t)configTICK_RATE_HZ);
}

#define OS_FREERTOS_IS_IN_ISR() (__get_IPSR() != 0U)


#endif // __OS_FREERTOS_H__
