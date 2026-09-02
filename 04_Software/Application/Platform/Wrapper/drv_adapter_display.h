#ifndef DRV_ADAPTER_DISPLAY_H
#define DRV_ADAPTER_DISPLAY_H

#include "bsp_display_xxx_handler.h"
#include <stdbool.h>

typedef struct { bsp_display_handler_t handler; } display_drv_t;
st7789t3_status_t drv_adapter_display_init(display_drv_t *display,
                                           const st7789t3_spi_driver_interface_t *spi,
                                           const st7789t3_control_interface_t *control,
                                           const st7789t3_delay_interface_t *delay);
st7789t3_status_t drv_adapter_display_set_window(display_drv_t *display, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
st7789t3_status_t drv_adapter_display_write_pixels(display_drv_t *display, const uint8_t *pixels, size_t size);
st7789t3_status_t drv_adapter_display_fill(display_drv_t *display, uint16_t color);

#endif /* DRV_ADAPTER_DISPLAY_H */
