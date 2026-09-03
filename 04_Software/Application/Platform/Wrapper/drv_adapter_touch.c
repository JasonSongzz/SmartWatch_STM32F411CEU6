#include "drv_adapter_touch.h"

#include <stddef.h>

static touch_drv_t s_dev[TOUCH_DEV_MAX];

bool drv_adapter_touch_reg(uint32_t index, const touch_drv_t *dev)
{
    if (index >= TOUCH_DEV_MAX || dev == NULL || dev->init == NULL ||
        dev->read == NULL || dev->get_info == NULL ||
        dev->sleep == NULL || dev->wakeup == NULL)
        return false;

    s_dev[index] = *dev;
    s_dev[index].idx = index;

    return true;
}

bool drv_adapter_touch_get_info(uint32_t index,
                                drv_adapter_touch_info_t *info)
{
    return index < TOUCH_DEV_MAX && info != NULL &&
           s_dev[index].get_info != NULL &&
           s_dev[index].get_info(&s_dev[index], info);
}

bool drv_adapter_touch_init(uint32_t index)
{
    return index < TOUCH_DEV_MAX && s_dev[index].init != NULL &&
           s_dev[index].init(&s_dev[index]);
}

drv_adapter_touch_status_t drv_adapter_touch_read(
    uint32_t index, drv_adapter_touch_point_t *point)
{
    if (index >= TOUCH_DEV_MAX || point == NULL ||
        s_dev[index].read == NULL)
        return DRV_ADAPTER_TOUCH_ERROR;

    return s_dev[index].read(&s_dev[index], point);
}

bool drv_adapter_touch_sleep(uint32_t index)
{
    return index < TOUCH_DEV_MAX && s_dev[index].sleep != NULL &&
           s_dev[index].sleep(&s_dev[index]);
}

bool drv_adapter_touch_wakeup(uint32_t index)
{
    return index < TOUCH_DEV_MAX && s_dev[index].wakeup != NULL &&
           s_dev[index].wakeup(&s_dev[index]);
}
