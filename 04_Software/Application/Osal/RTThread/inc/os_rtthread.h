#ifndef OS_RTTHREAD_H
#define OS_RTTHREAD_H

#include "common_types.h"
#include <rtthread.h>

static inline rt_int32_t os_rtthread_ms_to_ticks(osal_time_ms_t time_ms)
{
    rt_tick_t ticks;

    if (time_ms == OSAL_WAIT_FOREVER) return RT_WAITING_FOREVER;
    if (time_ms == 0U) return 0;
    ticks = rt_tick_from_millisecond((rt_int32_t)time_ms);
    return (rt_int32_t)(ticks == 0U ? 1U : ticks);
}

static inline osal_time_ms_t os_rtthread_ticks_to_ms(rt_tick_t ticks)
{
    return (osal_time_ms_t)(((uint64_t)ticks * 1000ULL) /
                            (uint64_t)RT_TICK_PER_SECOND);
}

static inline bool os_rtthread_is_in_isr(void)
{
    return rt_interrupt_get_nest() != 0U;
}

#endif /* OS_RTTHREAD_H */
