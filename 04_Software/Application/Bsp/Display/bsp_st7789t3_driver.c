#include "bsp_st7789t3_driver.h"

#define ST7789T3_IO_TIMEOUT_MS (1000U)

static display_status_t st7789t3_lock_spi(
    const display_spi_interface_t *spi)
{
    if (spi->pf_lock == NULL) return DISPLAY_OK;
    return spi->pf_lock(spi->bus_context, ST7789T3_IO_TIMEOUT_MS);
}

static display_status_t st7789t3_unlock_spi(
    const display_spi_interface_t *spi)
{
    if (spi->pf_unlock == NULL) return DISPLAY_OK;
    return spi->pf_unlock(spi->bus_context);
}

static display_status_t st7789t3_command_unlocked(
    bsp_display_driver_t *display, uint8_t command,
    const uint8_t *data, size_t size)
{
    const display_spi_interface_t *spi = display->spi;
    const display_control_interface_t *control = display->control;
    display_status_t status;

    control->pf_set_cs(control->context, false);
    control->pf_set_dc(control->context, false);
    status = spi->pf_spi_write(spi->bus_context, &command, 1U,
                               ST7789T3_IO_TIMEOUT_MS);

    if (status == DISPLAY_OK && size > 0U)
    {
        control->pf_set_dc(control->context, true);
        status = spi->pf_spi_write(spi->bus_context, data, size,
                                   ST7789T3_IO_TIMEOUT_MS);
    }

    control->pf_set_cs(control->context, true);
    return status;
}

static display_status_t st7789t3_command(
    bsp_display_driver_t *display, uint8_t command,
    const uint8_t *data, size_t size)
{
    const display_spi_interface_t *spi = display->spi;
    display_status_t status = st7789t3_lock_spi(spi);

    if (status != DISPLAY_OK) return status;
    status = st7789t3_command_unlocked(display, command, data, size);
    if (st7789t3_unlock_spi(spi) != DISPLAY_OK && status == DISPLAY_OK)
        status = DISPLAY_ERROR_RESOURCE;
    return status;
}

static display_status_t st7789t3_init(bsp_display_driver_t *display)
{
    uint8_t pixel_format = ST7789T3_PIXEL_FORMAT_RGB565;
    uint8_t madctl;

    if (display == NULL || display->spi == NULL || display->control == NULL ||
        display->delay == NULL || display->spi->pf_spi_write == NULL ||
        display->control->pf_set_cs == NULL ||
        display->control->pf_set_dc == NULL ||
        display->control->pf_set_reset == NULL ||
        display->delay->pf_delay_ms == NULL)
        return DISPLAY_ERROR_PARAMETER;

    display->width = ST7789T3_WIDTH;
    display->height = ST7789T3_HEIGHT;
    display->x_offset = ST7789T3_X_OFFSET;
    display->y_offset = ST7789T3_Y_OFFSET;
    display->madctl = ST7789T3_MADCTL;
    display->control->pf_set_cs(display->control->context, true);
    if (display->control->pf_set_backlight != NULL)
        display->control->pf_set_backlight(display->control->context, false);
    display->control->pf_set_reset(display->control->context, false);
    display->delay->pf_delay_ms(display->delay->context, 10U);
    display->control->pf_set_reset(display->control->context, true);
    display->delay->pf_delay_ms(display->delay->context, 120U);

    if (st7789t3_command(display, ST7789T3_CMD_SLPOUT, NULL, 0U) != DISPLAY_OK)
        return DISPLAY_ERROR;
    display->delay->pf_delay_ms(display->delay->context, 120U);

    if (st7789t3_command(display, ST7789T3_CMD_COLMOD, &pixel_format, 1U) != DISPLAY_OK)
        return DISPLAY_ERROR;

    madctl = display->madctl;
    if (st7789t3_command(display, ST7789T3_CMD_MADCTL, &madctl, 1U) != DISPLAY_OK ||
        st7789t3_command(display, ST7789T3_CMD_DISPON, NULL, 0U) != DISPLAY_OK)
        return DISPLAY_ERROR;

    if (display->control->pf_set_backlight != NULL)
        display->control->pf_set_backlight(display->control->context, true);
    display->initialized = true;
    return DISPLAY_OK;
}

static display_status_t st7789t3_set_window_unlocked(
    bsp_display_driver_t *display, uint16_t x0, uint16_t y0,
    uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    x0 += display->x_offset;
    x1 += display->x_offset;
    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;
    if (st7789t3_command_unlocked(
            display, ST7789T3_CMD_CASET, data, 4U) != DISPLAY_OK)
        return DISPLAY_ERROR;

    y0 += display->y_offset;
    y1 += display->y_offset;
    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    return st7789t3_command_unlocked(
        display, ST7789T3_CMD_RASET, data, 4U);
}

static display_status_t st7789t3_write_pixels_unlocked(
    bsp_display_driver_t *display, const uint8_t *pixels, size_t size)
{
    const display_spi_interface_t *spi = display->spi;
    const display_control_interface_t *control = display->control;
    const uint8_t command = ST7789T3_CMD_RAMWR;
    display_status_t status;

    control->pf_set_cs(control->context, false);
    control->pf_set_dc(control->context, false);
    status = spi->pf_spi_write(spi->bus_context, &command, 1U,
                               ST7789T3_IO_TIMEOUT_MS);
    control->pf_set_dc(control->context, true);

    if (status == DISPLAY_OK && spi->pf_spi_write_dma != NULL &&
        spi->pf_spi_wait_complete != NULL)
    {
        status = spi->pf_spi_write_dma(spi->bus_context, pixels, size);
        if (status == DISPLAY_OK)
            status = spi->pf_spi_wait_complete(spi->bus_context,
                                               ST7789T3_IO_TIMEOUT_MS);
    }
    else if (status == DISPLAY_OK)
    {
        status = spi->pf_spi_write(spi->bus_context, pixels, size,
                                   ST7789T3_IO_TIMEOUT_MS);
    }

    control->pf_set_cs(control->context, true);
    return status;
}

static display_status_t st7789t3_write_area(
    bsp_display_driver_t *display, uint16_t x0, uint16_t y0,
    uint16_t x1, uint16_t y1, const uint8_t *pixels, size_t size)
{
    const display_spi_interface_t *spi;
    display_status_t status;
    size_t expected_size;

    if (display == NULL || pixels == NULL || !display->initialized ||
        x0 > x1 || y0 > y1 || x1 >= display->width || y1 >= display->height)
        return DISPLAY_ERROR_PARAMETER;

    expected_size = (size_t)(x1 - x0 + 1U) * (size_t)(y1 - y0 + 1U) *
                    (ST7789T3_BITS_PER_PIXEL / 8U);
    if (size == 0U || size != expected_size) return DISPLAY_ERROR_PARAMETER;

    spi = display->spi;
    status = st7789t3_lock_spi(spi);
    if (status != DISPLAY_OK) return status;
    status = st7789t3_set_window_unlocked(display, x0, y0, x1, y1);
    if (status == DISPLAY_OK)
        status = st7789t3_write_pixels_unlocked(display, pixels, size);
    if (st7789t3_unlock_spi(spi) != DISPLAY_OK && status == DISPLAY_OK)
        status = DISPLAY_ERROR_RESOURCE;
    return status;
}

static display_status_t st7789t3_fill(bsp_display_driver_t *display, uint16_t color)
{
    uint8_t line[ST7789T3_WIDTH * 2U];
    size_t index;

    if (display == NULL || !display->initialized) return DISPLAY_ERROR_PARAMETER;

    for (index = 0U; index < sizeof(line); index += 2U)
    {
        line[index] = (uint8_t)(color >> 8);
        line[index + 1U] = (uint8_t)color;
    }

    for (index = 0U; index < display->height; index++)
    {
        if (st7789t3_write_area(display, 0U, (uint16_t)index,
                                display->width - 1U, (uint16_t)index,
                                line, sizeof(line)) != DISPLAY_OK)
            return DISPLAY_ERROR;
    }
    return DISPLAY_OK;
}

static display_status_t st7789t3_get_info(
    const bsp_display_driver_t *display, display_info_t *info)
{
    if (display == NULL || info == NULL || display->width == 0U ||
        display->height == 0U)
        return DISPLAY_ERROR_PARAMETER;

    *info = (display_info_t){
        .width = display->width,
        .height = display->height,
        .x_offset = display->x_offset,
        .y_offset = display->y_offset,
        .bits_per_pixel = ST7789T3_BITS_PER_PIXEL,
        .requires_byte_swap = ST7789T3_REQUIRES_BYTE_SWAP,
    };
    return DISPLAY_OK;
}

static display_status_t st7789t3_sleep(bsp_display_driver_t *display)
{
    if (display == NULL || !display->initialized) return DISPLAY_ERROR_PARAMETER;
    if (st7789t3_command(display, ST7789T3_CMD_DISPOFF, NULL, 0U) != DISPLAY_OK ||
        st7789t3_command(display, ST7789T3_CMD_SLPIN, NULL, 0U) != DISPLAY_OK)
        return DISPLAY_ERROR;

    if (display->control->pf_set_backlight != NULL)
        display->control->pf_set_backlight(display->control->context, false);
    display->initialized = false;
    return DISPLAY_OK;
}

static display_status_t st7789t3_wakeup(bsp_display_driver_t *display)
{
    if (display == NULL) return DISPLAY_ERROR_PARAMETER;
    return st7789t3_init(display);
}

display_status_t st7789t3_inst(bsp_display_driver_t *display,
                                const display_spi_interface_t *spi,
                                const display_control_interface_t *control,
                                const display_delay_interface_t *delay)
{
    if (display == NULL || spi == NULL || control == NULL || delay == NULL ||
        ((spi->pf_lock == NULL) != (spi->pf_unlock == NULL)))
        return DISPLAY_ERROR_PARAMETER;

    display->spi = spi;
    display->control = control;
    display->delay = delay;
    display->initialized = false;
    display->pf_init = st7789t3_init;
    display->pf_write_area = st7789t3_write_area;
    display->pf_fill = st7789t3_fill;
    display->pf_get_info = st7789t3_get_info;
    display->pf_sleep = st7789t3_sleep;
    display->pf_wakeup = st7789t3_wakeup;

    return st7789t3_init(display);
}
