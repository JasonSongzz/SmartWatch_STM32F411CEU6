#ifndef DRV_ADAPTER_DISPLAY_H
#define DRV_ADAPTER_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DISPLAY_DEV_MAX (1U)

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    uint8_t bits_per_pixel;
    bool requires_byte_swap;
} drv_adapter_display_info_t;

typedef struct display_drv
{
    uint32_t idx;
    void *user_data;
    bool (*init)(struct display_drv *dev);
    bool (*set_window)(struct display_drv *dev, uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1);
    bool (*write_pixels)(struct display_drv *dev,
                         const uint8_t *pixels, size_t size);
    bool (*fill)(struct display_drv *dev, uint16_t color);
    bool (*get_info)(struct display_drv *dev,
                     drv_adapter_display_info_t *info);
    bool (*sleep)(struct display_drv *dev);
    bool (*wakeup)(struct display_drv *dev);
} display_drv_t;

bool drv_adapter_display_reg(uint32_t index, const display_drv_t *dev);
bool drv_adapter_display_init(uint32_t index);
bool drv_adapter_display_set_window(uint32_t index,
                                    uint16_t x0, uint16_t y0,
                                    uint16_t x1, uint16_t y1);
bool drv_adapter_display_write_pixels(uint32_t index,
                                      const uint8_t *pixels, size_t size);
bool drv_adapter_display_fill(uint32_t index, uint16_t color);
bool drv_adapter_display_get_info(uint32_t index,
                                  drv_adapter_display_info_t *info);
bool drv_adapter_display_sleep(uint32_t index);
bool drv_adapter_display_wakeup(uint32_t index);

#endif /* DRV_ADAPTER_DISPLAY_H */
