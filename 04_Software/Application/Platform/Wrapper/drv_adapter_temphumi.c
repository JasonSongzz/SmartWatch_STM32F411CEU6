#include "drv_adapter_temphumi.h"
#include <stddef.h>

static temphumi_drv_t s_dev[TEMP_HUMI_DEV_MAX];

bool drv_adapter_temphumi_reg(uint32_t index, const temphumi_drv_t *dev)
{
    if (index >= TEMP_HUMI_DEV_MAX || dev == NULL || dev->init == NULL ||
        dev->refresh == NULL || dev->read_cached == NULL) {
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

void drv_adapter_temphumi_read_temp_and_humi(uint32_t index,
                                             float *temperature,
                                             float *humidity)
{
    if (index < TEMP_HUMI_DEV_MAX && s_dev[index].read_cached != NULL) {
        s_dev[index].read_cached(&s_dev[index], temperature, humidity);
    }
}
