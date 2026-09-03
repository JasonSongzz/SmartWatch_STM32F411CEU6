#ifndef BSP_DISPLAY_HANDLER_H
#define BSP_DISPLAY_HANDLER_H

/* Change only this include when replacing the display controller. */
#include "bsp_st7789t3_driver.h"

#include <stdbool.h>

typedef struct
{
    bsp_display_driver_t driver;
    bool initialized;
} bsp_display_handler_t;

display_status_t display_handler_init(
    bsp_display_handler_t *handler, const display_spi_interface_t *spi,
    const display_control_interface_t *control,
    const display_delay_interface_t *delay);
display_status_t display_handler_set_window(
    bsp_display_handler_t *handler, uint16_t x0, uint16_t y0,
    uint16_t x1, uint16_t y1);
display_status_t display_handler_write_pixels(
    bsp_display_handler_t *handler, const uint8_t *pixels, size_t size);
display_status_t display_handler_fill(bsp_display_handler_t *handler,
                                      uint16_t color);
display_status_t display_handler_get_info(
    const bsp_display_handler_t *handler, display_info_t *info);
display_status_t display_handler_sleep(bsp_display_handler_t *handler);
display_status_t display_handler_wakeup(bsp_display_handler_t *handler);

#endif /* BSP_DISPLAY_HANDLER_H */
