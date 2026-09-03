#include "drv_adapter_temphumi.h"
#include <stddef.h>

static temphumi_drv_t s_dev[TEMP_HUMI_DEV_MAX];

bool drv_adapter_temphumi_reg(uint32_t index, const temphumi_drv_t *dev)
{
    if (index >= TEMP_HUMI_DEV_MAX || dev == NULL || dev->init == NULL ||
        dev->refresh == NULL || dev->read_cached == NULL ||
        dev->sleep == NULL || dev->wakeup == NULL) {
        return false;
    }
    
    s_dev[index] = *dev;
    s_dev[index].idx = index;

    return true;
}

bool drv_adapter_temphumi_init(uint32_t index)
{
    return index < TEMP_HUMI_DEV_MAX && s_dev[index].init != NULL &&
           s_dev[index].init(&s_dev[index]);
}

bool drv_adapter_temphumi_refresh(uint32_t index)
{
    return index < TEMP_HUMI_DEV_MAX && s_dev[index].refresh != NULL &&
           s_dev[index].refresh(&s_dev[index]);
}

bool drv_adapter_temphumi_read_temp_and_humi(uint32_t index,
                                             float *temperature,
                                             float *humidity)
{
    return index < TEMP_HUMI_DEV_MAX && temperature != NULL &&
           humidity != NULL && s_dev[index].read_cached != NULL &&
           s_dev[index].read_cached(&s_dev[index], temperature, humidity);
}

bool drv_adapter_temphumi_sample(uint32_t index,
                                 float *temperature, float *humidity)
{
    return drv_adapter_temphumi_refresh(index) &&
           drv_adapter_temphumi_read_temp_and_humi(
               index, temperature, humidity);
}

bool drv_adapter_temphumi_sleep(uint32_t index)
{
    return index < TEMP_HUMI_DEV_MAX && s_dev[index].sleep != NULL &&
           s_dev[index].sleep(&s_dev[index]);
}

bool drv_adapter_temphumi_wakeup(uint32_t index)
{
    return index < TEMP_HUMI_DEV_MAX && s_dev[index].wakeup != NULL &&
           s_dev[index].wakeup(&s_dev[index]);
}
