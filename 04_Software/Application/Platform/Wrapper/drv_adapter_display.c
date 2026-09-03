#include "drv_adapter_display.h"

#include <stddef.h>

static display_drv_t s_dev[DISPLAY_DEV_MAX];

bool drv_adapter_display_reg(uint32_t index, const display_drv_t *dev)
{
    if (index >= DISPLAY_DEV_MAX || dev == NULL || dev->init == NULL ||
        dev->set_window == NULL || dev->write_pixels == NULL ||
        dev->fill == NULL || dev->get_info == NULL ||
        dev->sleep == NULL || dev->wakeup == NULL)
        return false;

    s_dev[index] = *dev;
    s_dev[index].idx = index;
    return true;
}

bool drv_adapter_display_init(uint32_t index)
{
    return index < DISPLAY_DEV_MAX && s_dev[index].init != NULL &&
           s_dev[index].init(&s_dev[index]);
}

bool drv_adapter_display_set_window(uint32_t index,
                                    uint16_t x0, uint16_t y0,
                                    uint16_t x1, uint16_t y1)
{
    return index < DISPLAY_DEV_MAX && s_dev[index].set_window != NULL &&
           s_dev[index].set_window(&s_dev[index], x0, y0, x1, y1);
}

bool drv_adapter_display_write_pixels(uint32_t index,
                                      const uint8_t *pixels, size_t size)
{
    return index < DISPLAY_DEV_MAX && pixels != NULL && size > 0U &&
           s_dev[index].write_pixels != NULL &&
           s_dev[index].write_pixels(&s_dev[index], pixels, size);
}

bool drv_adapter_display_fill(uint32_t index, uint16_t color)
{
    return index < DISPLAY_DEV_MAX && s_dev[index].fill != NULL &&
           s_dev[index].fill(&s_dev[index], color);
}

bool drv_adapter_display_get_info(uint32_t index,
                                  drv_adapter_display_info_t *info)
{
    return index < DISPLAY_DEV_MAX && info != NULL &&
           s_dev[index].get_info != NULL &&
           s_dev[index].get_info(&s_dev[index], info);
}

bool drv_adapter_display_sleep(uint32_t index)
{
    return index < DISPLAY_DEV_MAX && s_dev[index].sleep != NULL &&
           s_dev[index].sleep(&s_dev[index]);
}

bool drv_adapter_display_wakeup(uint32_t index)
{
    return index < DISPLAY_DEV_MAX && s_dev[index].wakeup != NULL &&
           s_dev[index].wakeup(&s_dev[index]);
}
