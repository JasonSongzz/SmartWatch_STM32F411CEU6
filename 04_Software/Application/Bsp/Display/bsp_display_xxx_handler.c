#include "bsp_display_xxx_handler.h"

#include <stddef.h>

st7789t3_status_t display_handler_init(bsp_display_handler_t *handler,
                                       const st7789t3_spi_driver_interface_t *spi,
                                       const st7789t3_control_interface_t *control,
                                       const st7789t3_delay_interface_t *delay)
{
    st7789t3_status_t status;

    if (handler == NULL || spi == NULL || control == NULL || delay == NULL)
        return ST7789T3_ERROR_PARAM;

    if (handler->initialized) return ST7789T3_OK;

    status = st7789t3_inst(&handler->driver, spi, control, delay);

    if (status == ST7789T3_OK) handler->initialized = true;

    return status;
}

st7789t3_status_t display_handler_set_window(bsp_display_handler_t *handler,
                                             uint16_t x0, uint16_t y0,
                                             uint16_t x1, uint16_t y1)
{
    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_set_window == NULL) return ST7789T3_ERROR_RESOURCE;
        
    return handler->driver.pf_set_window(&handler->driver, x0, y0, x1, y1);
}

st7789t3_status_t display_handler_write_pixels(bsp_display_handler_t *handler,
                                               const uint8_t *pixels, size_t size)
{
    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_write_pixels == NULL) return ST7789T3_ERROR_RESOURCE;

    return handler->driver.pf_write_pixels(&handler->driver, pixels, size);
}

st7789t3_status_t display_handler_fill(bsp_display_handler_t *handler,
                                      uint16_t color)
{
    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_fill == NULL) return ST7789T3_ERROR_RESOURCE;

    return handler->driver.pf_fill(&handler->driver, color);
}

st7789t3_status_t display_handler_sleep(bsp_display_handler_t *handler)
{
    st7789t3_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL) return ST7789T3_ERROR_RESOURCE;

    status = handler->driver.pf_sleep(&handler->driver);

    if (status == ST7789T3_OK) handler->initialized = false;

    return status;
}

st7789t3_status_t display_handler_wakeup(bsp_display_handler_t *handler)
{
    st7789t3_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return ST7789T3_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);

    if (status == ST7789T3_OK) handler->initialized = true;
    
    return status;
}
