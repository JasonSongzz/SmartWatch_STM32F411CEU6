#include "lvgl_display.h"

#include "drv_adapter_display.h"

#include <stddef.h>
#include <stdint.h>

#define LVGL_BUFFER_LINES  (20U)
#define LVGL_DISPLAY_INDEX (0U)

static void *s_draw_buffer;

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area,
                          uint8_t *px_map)
{
    uint16_t x0 = (uint16_t)area->x1;
    uint16_t y0 = (uint16_t)area->y1;
    uint16_t x1 = (uint16_t)area->x2;
    uint16_t y1 = (uint16_t)area->y2;
    size_t pixel_count = (size_t)(x1 - x0 + 1U) *
                         (size_t)(y1 - y0 + 1U);
    size_t size = pixel_count *
                  lv_color_format_get_size(
                      lv_display_get_color_format(display));

    (void)drv_adapter_display_write_area(LVGL_DISPLAY_INDEX,
                                         x0, y0, x1, y1, px_map, size);
    lv_display_flush_ready(display);
}

lv_display_t *lvgl_display_init(void)
{
    drv_adapter_display_info_t info;
    lv_color_format_t color_format;
    size_t buffer_size;
    lv_display_t *display;

    if (!drv_adapter_display_init(LVGL_DISPLAY_INDEX) ||
        !drv_adapter_display_get_info(LVGL_DISPLAY_INDEX, &info) ||
        info.width == 0U || info.height == 0U ||
        info.bits_per_pixel != 16U)
        return NULL;

    color_format = info.requires_byte_swap
                 ? LV_COLOR_FORMAT_RGB565_SWAPPED
                 : LV_COLOR_FORMAT_RGB565;
    buffer_size = (size_t)info.width * LVGL_BUFFER_LINES *
                  lv_color_format_get_size(color_format);
    s_draw_buffer = lv_malloc(buffer_size);
    if (s_draw_buffer == NULL) return NULL;

    display = lv_display_create(info.width, info.height);
    if (display == NULL)
    {
        lv_free(s_draw_buffer);
        s_draw_buffer = NULL;
        return NULL;
    }

    lv_display_set_color_format(display, color_format);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_set_buffers(display, s_draw_buffer, NULL, buffer_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return display;
}
