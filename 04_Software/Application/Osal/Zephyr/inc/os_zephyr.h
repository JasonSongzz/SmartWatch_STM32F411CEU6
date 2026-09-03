#ifndef OS_ZEPHYR_H
#define OS_ZEPHYR_H

#include "common_types.h"
#include <limits.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>

static inline k_timeout_t os_zephyr_timeout(osal_time_ms_t time_ms)
{
    if (time_ms == OSAL_WAIT_FOREVER) return K_FOREVER;
    if (time_ms == OSAL_WAIT_NONE) return K_NO_WAIT;
    return K_MSEC(time_ms);
}

#endif /* OS_ZEPHYR_H */
