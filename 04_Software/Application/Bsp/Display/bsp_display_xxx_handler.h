#ifndef BSP_DISPLAY_HANDLER_H
#define BSP_DISPLAY_HANDLER_H

#include "bsp_st7789t3_driver.h"
#include <stdbool.h>

typedef struct
{
    bsp_st7789t3_t driver;
    bool initialized;
} bsp_display_handler_t;

st7789t3_status_t display_handler_init(bsp_display_handler_t *handler,
                                       const st7789t3_spi_driver_interface_t *spi,
                                       const st7789t3_control_interface_t *control,
                                       const st7789t3_delay_interface_t *delay);
st7789t3_status_t display_handler_set_window(bsp_display_handler_t *handler,
                                             uint16_t x0, uint16_t y0,
                                             uint16_t x1, uint16_t y1);
st7789t3_status_t display_handler_write_pixels(bsp_display_handler_t *handler,
                                               const uint8_t *pixels, size_t size);
st7789t3_status_t display_handler_fill(bsp_display_handler_t *handler,
                                      uint16_t color);
st7789t3_status_t display_handler_sleep(bsp_display_handler_t *handler);
st7789t3_status_t display_handler_wakeup(bsp_display_handler_t *handler);

#endif /* BSP_DISPLAY_HANDLER_H */
