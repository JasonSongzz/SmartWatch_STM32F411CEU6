#ifndef DRV_ADAPTER_ACCEL_H
#define DRV_ADAPTER_ACCEL_H

#include <stdbool.h>
#include <stdint.h>

#define ACCEL_DEV_MAX (2U)

typedef struct accel_drv
{
    uint32_t idx;
    void *user_data;
    bool (*init)(struct accel_drv *dev);
    bool (*refresh)(struct accel_drv *dev);
    void (*read_cached)(struct accel_drv *dev, float *x, float *y, float *z);
    bool (*sleep)(struct accel_drv *dev);
    bool (*wakeup)(struct accel_drv *dev);
} accel_drv_t;

bool drv_adapter_accel_reg(uint32_t index, const accel_drv_t *dev);
bool drv_adapter_accel_init(uint32_t index);
bool drv_adapter_accel_refresh(uint32_t index);
bool drv_adapter_accel_sleep(uint32_t index);
bool drv_adapter_accel_wakeup(uint32_t index);
void drv_adapter_accel_read(uint32_t index, float *x, float *y, float *z);

#endif /* DRV_ADAPTER_ACCEL_H */
