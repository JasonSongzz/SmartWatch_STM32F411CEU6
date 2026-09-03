#include "lvgl_port_display.h"

#include "drv_adapter_port_display.h"
#include "spi.h"

#define LVGL_BUFFER_LINES   20U
#define LVGL_DISPLAY_INDEX  0U

/* Replace these defaults with the actual LCD pin mapping. */
#ifndef LCD_CS_PORT
#define LCD_CS_PORT GPIOA
#endif
#ifndef LCD_CS_PIN
#define LCD_CS_PIN GPIO_PIN_4
#endif
#ifndef LCD_DC_PORT
#define LCD_DC_PORT GPIOB
#endif
#ifndef LCD_DC_PIN
#define LCD_DC_PIN GPIO_PIN_0
#endif
#ifndef LCD_RESET_PORT
#define LCD_RESET_PORT GPIOB
#endif
#ifndef LCD_RESET_PIN
#define LCD_RESET_PIN GPIO_PIN_1
#endif
#ifndef LCD_BL_PORT
#define LCD_BL_PORT GPIOB
#endif
#ifndef LCD_BL_PIN
#define LCD_BL_PIN GPIO_PIN_2
#endif

static display_port_config_t s_display_config = {
    .spi = &hspi1,
    .cs_port = LCD_CS_PORT, .cs_pin = LCD_CS_PIN,
    .dc_port = LCD_DC_PORT, .dc_pin = LCD_DC_PIN,
    .reset_port = LCD_RESET_PORT, .reset_pin = LCD_RESET_PIN,
    .bl_port = LCD_BL_PORT, .bl_pin = LCD_BL_PIN,
};
static void *s_draw_buffer;

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t x0 = (uint16_t)area->x1;
    uint16_t y0 = (uint16_t)area->y1;
    uint16_t x1 = (uint16_t)area->x2;
    uint16_t y1 = (uint16_t)area->y2;
    size_t pixel_count = (size_t)(x1 - x0 + 1U) * (size_t)(y1 - y0 + 1U);
    size_t size = pixel_count * lv_color_format_get_size(lv_display_get_color_format(display));

    if (!drv_adapter_display_set_window(LVGL_DISPLAY_INDEX,
                                        x0, y0, x1, y1) ||
        !drv_adapter_display_write_pixels(LVGL_DISPLAY_INDEX, px_map, size))
    {
        lv_display_flush_ready(display);
        return;
    }
    lv_display_flush_ready(display);
}

lv_display_t *lvgl_port_display_init(void)
{
    drv_adapter_display_info_t info;
    lv_color_format_t color_format;
    size_t buffer_size;
    lv_display_t *display;

    if (!drv_adapter_port_display_register(LVGL_DISPLAY_INDEX,
                                           &s_display_config) ||
        !drv_adapter_display_init(LVGL_DISPLAY_INDEX) ||
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
