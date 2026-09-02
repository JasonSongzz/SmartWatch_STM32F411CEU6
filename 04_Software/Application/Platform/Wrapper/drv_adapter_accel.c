#include "drv_adapter_accel.h"

#include <stddef.h>

static accel_drv_t s_dev[ACCEL_DEV_MAX];

bool drv_adapter_accel_reg(uint32_t index, const accel_drv_t *dev)
{
    if (index >= ACCEL_DEV_MAX || dev == NULL || dev->init == NULL ||
        dev->refresh == NULL || dev->read_cached == NULL ||
        dev->sleep == NULL || dev->wakeup == NULL)
    {
        return false;
    }
    
    s_dev[index] = *dev;
    s_dev[index].idx = index;

    return true;
}

bool drv_adapter_accel_init(uint32_t index)
{
    return index < ACCEL_DEV_MAX && s_dev[index].init != NULL &&
           s_dev[index].init(&s_dev[index]);
}

bool drv_adapter_accel_refresh(uint32_t index)
{
    return index < ACCEL_DEV_MAX && s_dev[index].refresh != NULL &&
           s_dev[index].refresh(&s_dev[index]);
}

bool drv_adapter_accel_sleep(uint32_t index)
{
    return index < ACCEL_DEV_MAX && s_dev[index].sleep != NULL &&
           s_dev[index].sleep(&s_dev[index]);
}

bool drv_adapter_accel_wakeup(uint32_t index)
{
    return index < ACCEL_DEV_MAX && s_dev[index].wakeup != NULL &&
           s_dev[index].wakeup(&s_dev[index]);
}

void drv_adapter_accel_read(uint32_t index, float *x, float *y, float *z)
{
    if (index < ACCEL_DEV_MAX && s_dev[index].read_cached != NULL)
    {
        s_dev[index].read_cached(&s_dev[index], x, y, z);
    }
}
