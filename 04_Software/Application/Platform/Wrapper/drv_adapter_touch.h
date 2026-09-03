#ifndef DRV_ADAPTER_TOUCH_H
#define DRV_ADAPTER_TOUCH_H

#include <stdbool.h>
#include <stdint.h>

#define TOUCH_DEV_MAX (2U)

typedef enum
{
    DRV_ADAPTER_TOUCH_OK = 0,
    DRV_ADAPTER_TOUCH_NO_TOUCH,
    DRV_ADAPTER_TOUCH_ERROR
} drv_adapter_touch_status_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t gesture;
    uint8_t event;
    uint8_t fingers;
} drv_adapter_touch_point_t;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint8_t max_points;
} drv_adapter_touch_info_t;

typedef struct touch_drv
{
    uint32_t idx;
    void *user_data;
    bool (*init)(struct touch_drv *dev);
    drv_adapter_touch_status_t (*read)(
        struct touch_drv *dev, drv_adapter_touch_point_t *point);
    bool (*get_info)(struct touch_drv *dev,
                     drv_adapter_touch_info_t *info);
    bool (*sleep)(struct touch_drv *dev);
    bool (*wakeup)(struct touch_drv *dev);
} touch_drv_t;

bool drv_adapter_touch_reg(uint32_t index, const touch_drv_t *dev);
bool drv_adapter_touch_init(uint32_t index);
drv_adapter_touch_status_t drv_adapter_touch_read(
    uint32_t index, drv_adapter_touch_point_t *point);
bool drv_adapter_touch_get_info(uint32_t index,
                                drv_adapter_touch_info_t *info);
bool drv_adapter_touch_sleep(uint32_t index);
bool drv_adapter_touch_wakeup(uint32_t index);

#endif /* DRV_ADAPTER_TOUCH_H */
