#include "drv_adapter_display.h"

st7789t3_status_t drv_adapter_display_init(display_drv_t *display,
                                           const st7789t3_spi_driver_interface_t *spi,
                                           const st7789t3_control_interface_t *control,
                                           const st7789t3_delay_interface_t *delay)
{
    if (display == NULL) return ST7789T3_ERROR_PARAM;
    return display_handler_init(&display->handler, spi, control, delay);
}

st7789t3_status_t drv_adapter_display_set_window(display_drv_t *display, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{ return display_handler_set_window(display != NULL ? &display->handler : NULL, x0, y0, x1, y1); }

st7789t3_status_t drv_adapter_display_write_pixels(display_drv_t *display, const uint8_t *pixels, size_t size)
{ return display_handler_write_pixels(display != NULL ? &display->handler : NULL, pixels, size); }

st7789t3_status_t drv_adapter_display_fill(display_drv_t *display, uint16_t color)
{ return display_handler_fill(display != NULL ? &display->handler : NULL, color); }
