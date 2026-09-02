#ifndef DRV_ADAPTER_TEMPHUMI_H
#define DRV_ADAPTER_TEMPHUMI_H

#include <stdbool.h>
#include <stdint.h>

#define TEMP_HUMI_DEV_MAX (2U)

typedef struct temphumi_drv {
    uint32_t idx;
    void *user_data;
    bool (*init)(struct temphumi_drv *dev);
    bool (*refresh)(struct temphumi_drv *dev);
    void (*read_cached)(struct temphumi_drv *dev, float *temp, float *humi);
} temphumi_drv_t;

bool drv_adapter_temphumi_reg(uint32_t index, const temphumi_drv_t *dev);
bool drv_adapter_temphumi_init(uint32_t index);
bool drv_adapter_temphumi_refresh(uint32_t index);
void drv_adapter_temphumi_read_temp_and_humi(uint32_t index,
                                             float *temperature,
                                             float *humidity);

#endif /* DRV_ADAPTER_TEMP_HUMI_H */
