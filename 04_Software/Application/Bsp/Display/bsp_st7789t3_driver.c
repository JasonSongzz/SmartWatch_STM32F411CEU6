#include "bsp_st7789t3_driver.h"

#define ST7789T3_IO_TIMEOUT_MS (1000U)

static st7789t3_status_t __lock_spi(
    const st7789t3_spi_driver_interface_t *spi)
{
    if (spi->pf_lock == NULL) return ST7789T3_OK;
    return spi->pf_lock(spi->bus_context, ST7789T3_IO_TIMEOUT_MS);
}

static st7789t3_status_t __unlock_spi(
    const st7789t3_spi_driver_interface_t *spi)
{
    if (spi->pf_unlock == NULL) return ST7789T3_OK;
    return spi->pf_unlock(spi->bus_context);
}

static st7789t3_status_t __command(bsp_st7789t3_t *display, uint8_t command,
                                    const uint8_t *data, size_t size)
{
    const st7789t3_spi_driver_interface_t *spi = display->spi;
    const st7789t3_control_interface_t *control = display->control;
    st7789t3_status_t status;

    status = __lock_spi(spi);
    if (status != ST7789T3_OK) return status;

    control->pf_set_cs(control->context, false);
    control->pf_set_dc(control->context, false);
    status = spi->pf_spi_write(spi->bus_context, &command, 1U,
                               ST7789T3_IO_TIMEOUT_MS);

    if (status == ST7789T3_OK && size > 0U)
    {
        control->pf_set_dc(control->context, true);
        status = spi->pf_spi_write(spi->bus_context, data, size,
                                   ST7789T3_IO_TIMEOUT_MS);
    }

    control->pf_set_cs(control->context, true);
    if (__unlock_spi(spi) != ST7789T3_OK && status == ST7789T3_OK)
        status = ST7789T3_ERROR_RESOURCE;
    return status;
}

static st7789t3_status_t st7789t3_init(bsp_st7789t3_t *display)
{
    uint8_t pixel_format = ST7789T3_PIXEL_FORMAT_RGB565;
    uint8_t madctl;

    if (display == NULL || display->spi == NULL || display->control == NULL ||
        display->delay == NULL || display->spi->pf_spi_write == NULL ||
        display->control->pf_set_cs == NULL ||
        display->control->pf_set_dc == NULL ||
        display->control->pf_set_reset == NULL ||
        display->delay->pf_delay_ms == NULL)
        return ST7789T3_ERROR_PARAM;

    display->width = 240U;
    display->height = 280U;
    display->x_offset = 0U;
    display->y_offset = 0U;
    display->madctl = 0x00U;
    display->control->pf_set_cs(display->control->context, true);
    if (display->control->pf_set_backlight != NULL)
        display->control->pf_set_backlight(display->control->context, false);
    display->control->pf_set_reset(display->control->context, false);
    display->delay->pf_delay_ms(display->delay->context, 10U);
    display->control->pf_set_reset(display->control->context, true);
    display->delay->pf_delay_ms(display->delay->context, 120U);

    if (__command(display, ST7789T3_CMD_SLPOUT, NULL, 0U) != ST7789T3_OK)
        return ST7789T3_ERROR;
    display->delay->pf_delay_ms(display->delay->context, 120U);

    if (__command(display, ST7789T3_CMD_COLMOD, &pixel_format, 1U) != ST7789T3_OK)
        return ST7789T3_ERROR;

    madctl = display->madctl;
    if (__command(display, ST7789T3_CMD_MADCTL, &madctl, 1U) != ST7789T3_OK ||
        __command(display, ST7789T3_CMD_DISPON, NULL, 0U) != ST7789T3_OK)
        return ST7789T3_ERROR;

    if (display->control->pf_set_backlight != NULL)
        display->control->pf_set_backlight(display->control->context, true);
    display->initialized = true;
    return ST7789T3_OK;
}

static st7789t3_status_t st7789t3_set_window(bsp_st7789t3_t *display,
                                              uint16_t x0, uint16_t y0,
                                              uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    if (display == NULL || !display->initialized || x0 > x1 || y0 > y1 ||
        x1 >= display->width || y1 >= display->height)
        return ST7789T3_ERROR_PARAM;

    x0 += display->x_offset;
    x1 += display->x_offset;
    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;
    if (__command(display, ST7789T3_CMD_CASET, data, 4U) != ST7789T3_OK)
        return ST7789T3_ERROR;

    y0 += display->y_offset;
    y1 += display->y_offset;
    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    return __command(display, ST7789T3_CMD_RASET, data, 4U);
}

static st7789t3_status_t st7789t3_write_pixels(bsp_st7789t3_t *display,
                                                const uint8_t *pixels,
                                                size_t size)
{
    const st7789t3_spi_driver_interface_t *spi;
    const st7789t3_control_interface_t *control;
    const uint8_t command = ST7789T3_CMD_RAMWR;
    st7789t3_status_t status;

    if (display == NULL || pixels == NULL || size == 0U || !display->initialized)
        return ST7789T3_ERROR_PARAM;

    spi = display->spi;
    control = display->control;
    status = __lock_spi(spi);
    if (status != ST7789T3_OK) return status;

    control->pf_set_cs(control->context, false);
    control->pf_set_dc(control->context, false);
    status = spi->pf_spi_write(spi->bus_context, &command, 1U,
                               ST7789T3_IO_TIMEOUT_MS);
    control->pf_set_dc(control->context, true);

    if (status == ST7789T3_OK && spi->pf_spi_write_dma != NULL &&
        spi->pf_spi_wait_complete != NULL)
    {
        status = spi->pf_spi_write_dma(spi->bus_context, pixels, size);
        if (status == ST7789T3_OK)
            status = spi->pf_spi_wait_complete(spi->bus_context,
                                               ST7789T3_IO_TIMEOUT_MS);
    }
    else if (status == ST7789T3_OK)
    {
        status = spi->pf_spi_write(spi->bus_context, pixels, size,
                                   ST7789T3_IO_TIMEOUT_MS);
    }

    control->pf_set_cs(control->context, true);
    if (__unlock_spi(spi) != ST7789T3_OK && status == ST7789T3_OK)
        status = ST7789T3_ERROR_RESOURCE;
    return status;
}

static st7789t3_status_t st7789t3_fill(bsp_st7789t3_t *display, uint16_t color)
{
    uint8_t line[240U * 2U];
    size_t index;

    if (display == NULL || !display->initialized) return ST7789T3_ERROR_PARAM;

    for (index = 0U; index < sizeof(line); index += 2U)
    {
        line[index] = (uint8_t)(color >> 8);
        line[index + 1U] = (uint8_t)color;
    }

    if (st7789t3_set_window(display, 0U, 0U, 239U, 279U) != ST7789T3_OK)
        return ST7789T3_ERROR;

    for (index = 0U; index < 280U; index++)
    {
        if (st7789t3_write_pixels(display, line, sizeof(line)) != ST7789T3_OK)
            return ST7789T3_ERROR;
    }
    return ST7789T3_OK;
}

static st7789t3_status_t st7789t3_sleep(bsp_st7789t3_t *display)
{
    if (display == NULL || !display->initialized) return ST7789T3_ERROR_PARAM;
    if (__command(display, ST7789T3_CMD_DISPOFF, NULL, 0U) != ST7789T3_OK ||
        __command(display, ST7789T3_CMD_SLPIN, NULL, 0U) != ST7789T3_OK)
        return ST7789T3_ERROR;

    if (display->control->pf_set_backlight != NULL)
        display->control->pf_set_backlight(display->control->context, false);
    display->initialized = false;
    return ST7789T3_OK;
}

static st7789t3_status_t st7789t3_wakeup(bsp_st7789t3_t *display)
{
    if (display == NULL) return ST7789T3_ERROR_PARAM;
    return st7789t3_init(display);
}

st7789t3_status_t st7789t3_inst(bsp_st7789t3_t *display,
                                const st7789t3_spi_driver_interface_t *spi,
                                const st7789t3_control_interface_t *control,
                                const st7789t3_delay_interface_t *delay)
{
    if (display == NULL || spi == NULL || control == NULL || delay == NULL ||
        ((spi->pf_lock == NULL) != (spi->pf_unlock == NULL)))
        return ST7789T3_ERROR_PARAM;

    display->spi = spi;
    display->control = control;
    display->delay = delay;
    display->initialized = false;
    display->pf_init = st7789t3_init;
    display->pf_set_window = st7789t3_set_window;
    display->pf_write_pixels = st7789t3_write_pixels;
    display->pf_fill = st7789t3_fill;
    display->pf_sleep = st7789t3_sleep;
    display->pf_wakeup = st7789t3_wakeup;

    return st7789t3_init(display);
}
