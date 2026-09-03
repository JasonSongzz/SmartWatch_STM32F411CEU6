#include "bsp_display_handler.h"

#include <stddef.h>
#include <string.h>

display_status_t display_handler_init(
    bsp_display_handler_t *handler, const display_spi_interface_t *spi,
    const display_control_interface_t *control,
    const display_delay_interface_t *delay)
{
    display_status_t status;

    if (handler == NULL || spi == NULL || control == NULL || delay == NULL)
        return DISPLAY_ERROR_PARAMETER;

    memset(handler, 0, sizeof(*handler));
    status = bsp_display_inst(&handler->driver, spi, control, delay);
    handler->initialized = status == DISPLAY_OK;
    return status;
}

display_status_t display_handler_set_window(
    bsp_display_handler_t *handler, uint16_t x0, uint16_t y0,
    uint16_t x1, uint16_t y1)
{
    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_set_window == NULL)
        return DISPLAY_ERROR_RESOURCE;

    return handler->driver.pf_set_window(&handler->driver,
                                         x0, y0, x1, y1);
}

display_status_t display_handler_write_pixels(
    bsp_display_handler_t *handler, const uint8_t *pixels, size_t size)
{
    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_write_pixels == NULL)
        return DISPLAY_ERROR_RESOURCE;

    return handler->driver.pf_write_pixels(&handler->driver, pixels, size);
}

display_status_t display_handler_fill(bsp_display_handler_t *handler,
                                      uint16_t color)
{
    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_fill == NULL)
        return DISPLAY_ERROR_RESOURCE;

    return handler->driver.pf_fill(&handler->driver, color);
}

display_status_t display_handler_get_info(
    const bsp_display_handler_t *handler, display_info_t *info)
{
    if (handler == NULL || info == NULL || !handler->initialized ||
        handler->driver.pf_get_info == NULL)
        return DISPLAY_ERROR_RESOURCE;

    return handler->driver.pf_get_info(&handler->driver, info);
}

display_status_t display_handler_sleep(bsp_display_handler_t *handler)
{
    display_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL)
        return DISPLAY_ERROR_RESOURCE;

    status = handler->driver.pf_sleep(&handler->driver);
    if (status == DISPLAY_OK) handler->initialized = false;
    return status;
}

display_status_t display_handler_wakeup(bsp_display_handler_t *handler)
{
    display_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return DISPLAY_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);
    if (status == DISPLAY_OK) handler->initialized = true;
    return status;
}
