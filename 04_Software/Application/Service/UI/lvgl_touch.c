#include "lvgl_touch.h"

#include "drv_adapter_touch.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#define LVGL_TOUCH_INDEX                  (0U)
#define LVGL_TOUCH_READ_PERIOD_MS         (10U)
#define LVGL_TOUCH_MAX_CONSECUTIVE_ERRORS (3U)

typedef struct
{
    lv_point_t last_point;
    uint8_t error_count;
    bool pressed;
} lvgl_touch_context_t;

static lvgl_touch_context_t s_touch_context;

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    drv_adapter_touch_point_t point;
    drv_adapter_touch_status_t status;

    (void)indev;
    status = drv_adapter_touch_read(LVGL_TOUCH_INDEX, &point);
    if (status == DRV_ADAPTER_TOUCH_OK)
    {
        s_touch_context.error_count = 0U;
        if (point.event == DRV_ADAPTER_TOUCH_EVENT_UP || point.fingers == 0U)
        {
            s_touch_context.pressed = false;
        }
        else
        {
            s_touch_context.last_point.x = (lv_coord_t)point.x;
            s_touch_context.last_point.y = (lv_coord_t)point.y;
            s_touch_context.pressed = true;
        }
    }
    else if (status == DRV_ADAPTER_TOUCH_NO_TOUCH)
    {
        s_touch_context.error_count = 0U;
        s_touch_context.pressed = false;
    }
    else if (++s_touch_context.error_count >=
             LVGL_TOUCH_MAX_CONSECUTIVE_ERRORS)
    {
        s_touch_context.pressed = false;
    }

    data->point = s_touch_context.last_point;
    data->state = s_touch_context.pressed
                ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

lv_indev_t *lvgl_touch_init(lv_display_t *display)
{
    drv_adapter_touch_processing_config_t config;
    int32_t horizontal_resolution;
    int32_t vertical_resolution;
    lv_indev_t *indev;
    lv_timer_t *read_timer;

    if (display == NULL || !drv_adapter_touch_init(LVGL_TOUCH_INDEX) ||
        !drv_adapter_touch_get_processing_config(LVGL_TOUCH_INDEX, &config))
        return NULL;

    horizontal_resolution = lv_display_get_horizontal_resolution(display);
    vertical_resolution = lv_display_get_vertical_resolution(display);
    if (horizontal_resolution <= 0 || vertical_resolution <= 0 ||
        horizontal_resolution > UINT16_MAX || vertical_resolution > UINT16_MAX)
        return NULL;

    config.output_width = (uint16_t)horizontal_resolution;
    config.output_height = (uint16_t)vertical_resolution;
    if (!drv_adapter_touch_set_processing_config(LVGL_TOUCH_INDEX, &config))
        return NULL;

    s_touch_context = (lvgl_touch_context_t){0};
    indev = lv_indev_create();
    if (indev == NULL) return NULL;

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);
    lv_indev_set_display(indev, display);
    read_timer = lv_indev_get_read_timer(indev);
    if (read_timer != NULL)
        lv_timer_set_period(read_timer, LVGL_TOUCH_READ_PERIOD_MS);
    return indev;
}
