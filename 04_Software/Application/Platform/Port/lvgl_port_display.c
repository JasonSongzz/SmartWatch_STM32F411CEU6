#include "lvgl_port_display.h"

#include "spi.h"

#define LVGL_DISPLAY_WIDTH  240U
#define LVGL_DISPLAY_HEIGHT 280U
#define LVGL_BUFFER_LINES   20U

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

static display_drv_t s_display;
static display_port_config_t s_display_config = {
    .spi = &hspi1,
    .cs_port = LCD_CS_PORT, .cs_pin = LCD_CS_PIN,
    .dc_port = LCD_DC_PORT, .dc_pin = LCD_DC_PIN,
    .reset_port = LCD_RESET_PORT, .reset_pin = LCD_RESET_PIN,
    .bl_port = LCD_BL_PORT, .bl_pin = LCD_BL_PIN,
};
/* LVGL 9 lv_color_t is RGB888; the display buffer is explicitly RGB565. */
static uint16_t s_draw_buffer[LVGL_DISPLAY_WIDTH * LVGL_BUFFER_LINES];

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    uint16_t x0 = (uint16_t)area->x1;
    uint16_t y0 = (uint16_t)area->y1;
    uint16_t x1 = (uint16_t)area->x2;
    uint16_t y1 = (uint16_t)area->y2;
    size_t pixel_count = (size_t)(x1 - x0 + 1U) * (size_t)(y1 - y0 + 1U);
    size_t size = pixel_count * lv_color_format_get_size(lv_display_get_color_format(display));

    if (drv_adapter_display_set_window(&s_display, x0, y0, x1, y1) != ST7789T3_OK ||
        drv_adapter_display_write_pixels(&s_display, px_map, size) != ST7789T3_OK)
    {
        lv_display_flush_ready(display);
        return;
    }
    lv_display_flush_ready(display);
}

lv_display_t *lvgl_port_display_init(void)
{
    lv_display_t *display;
    if (drv_adapter_port_display_register(&s_display, &s_display_config) != ST7789T3_OK)
        return NULL;
    display = lv_display_create(LVGL_DISPLAY_WIDTH, LVGL_DISPLAY_HEIGHT);
    if (display == NULL) return NULL;
    /* ST7789 receives the high byte first on its 8-bit SPI interface. */
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_set_buffers(display, s_draw_buffer, NULL, sizeof(s_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return display;
}
