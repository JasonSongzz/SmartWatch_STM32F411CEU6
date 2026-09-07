#include "drv_adapter_accel.h"

#include <stddef.h>

static accel_drv_t s_dev[ACCEL_DEV_MAX];

bool drv_adapter_accel_reg(uint32_t index, const accel_drv_t *dev)
{
    if (index >= ACCEL_DEV_MAX || dev == NULL || dev->init == NULL ||
        dev->refresh == NULL || dev->read_cached == NULL ||
        dev->read_snapshot == NULL || dev->set_calibration == NULL ||
        dev->get_calibration == NULL || dev->set_filter == NULL ||
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

bool drv_adapter_accel_read(uint32_t index, float *x, float *y, float *z)
{
    return index < ACCEL_DEV_MAX && x != NULL && y != NULL && z != NULL &&
           s_dev[index].read_cached != NULL &&
           s_dev[index].read_cached(&s_dev[index], x, y, z);
}

bool drv_adapter_accel_sample(uint32_t index, float *x, float *y, float *z)
{
    return drv_adapter_accel_refresh(index) &&
           drv_adapter_accel_read(index, x, y, z);
}

bool drv_adapter_accel_read_snapshot(uint32_t index,
                                     accel_snapshot_t *snapshot)
{
    return index < ACCEL_DEV_MAX && snapshot != NULL &&
           s_dev[index].read_snapshot != NULL &&
           s_dev[index].read_snapshot(&s_dev[index], snapshot);
}

bool drv_adapter_accel_set_calibration(
    uint32_t index, const accel_calibration_t *calibration)
{
    return index < ACCEL_DEV_MAX && calibration != NULL &&
           s_dev[index].set_calibration != NULL &&
           s_dev[index].set_calibration(&s_dev[index], calibration);
}

bool drv_adapter_accel_get_calibration(
    uint32_t index, accel_calibration_t *calibration)
{
    return index < ACCEL_DEV_MAX && calibration != NULL &&
           s_dev[index].get_calibration != NULL &&
           s_dev[index].get_calibration(&s_dev[index], calibration);
}

bool drv_adapter_accel_set_filter(
    uint32_t index, const accel_filter_config_t *config)
{
    return index < ACCEL_DEV_MAX && config != NULL &&
           s_dev[index].set_filter != NULL &&
           s_dev[index].set_filter(&s_dev[index], config);
}
