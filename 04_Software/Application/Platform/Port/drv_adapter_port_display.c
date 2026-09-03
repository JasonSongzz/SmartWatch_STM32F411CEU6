#include "drv_adapter_port_display.h"

#include "bsp_display_handler.h"
#include "osal.h"

#define DISPLAY_IO_TIMEOUT_MS (1000U)

typedef struct
{
    bool registered;
    display_port_config_t config;
    osal_mutex_handle_t mutex;
    display_spi_interface_t spi_interface;
    display_control_interface_t control_interface;
    display_delay_interface_t delay_interface;
    bsp_display_handler_t handler;
} display_port_context_t;

static display_port_context_t s_port[DISPLAY_DEV_MAX];

static void enable_gpio_clock(GPIO_TypeDef *gpio)
{
    if (gpio == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (gpio == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (gpio == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
#ifdef GPIOD
    else if (gpio == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
#ifdef GPIOE
    else if (gpio == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
#ifdef GPIOF
    else if (gpio == GPIOF) __HAL_RCC_GPIOF_CLK_ENABLE();
#endif
#ifdef GPIOG
    else if (gpio == GPIOG) __HAL_RCC_GPIOG_CLK_ENABLE();
#endif
#ifdef GPIOH
    else if (gpio == GPIOH) __HAL_RCC_GPIOH_CLK_ENABLE();
#endif
}

static display_status_t display_status_from_hal(HAL_StatusTypeDef status)
{
    if (status == HAL_OK) return DISPLAY_OK;
    if (status == HAL_TIMEOUT) return DISPLAY_ERROR_TIMEOUT;
    if (status == HAL_BUSY) return DISPLAY_ERROR_RESOURCE;
    return DISPLAY_ERROR;
}

static void display_cs(void *context, bool high)
{
    display_port_context_t *port = context;
    HAL_GPIO_WritePin(port->config.cs_port, port->config.cs_pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void display_dc(void *context, bool data_mode)
{
    display_port_context_t *port = context;
    HAL_GPIO_WritePin(port->config.dc_port, port->config.dc_pin,
                      data_mode ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void display_reset(void *context, bool high)
{
    display_port_context_t *port = context;
    HAL_GPIO_WritePin(port->config.reset_port, port->config.reset_pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void display_backlight(void *context, bool on)
{
    display_port_context_t *port = context;

    if (port->config.bl_port != NULL && port->config.bl_pin != 0U)
        HAL_GPIO_WritePin(port->config.bl_port, port->config.bl_pin,
                          on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static display_status_t display_lock(void *context, uint32_t timeout_ms)
{
    display_port_context_t *port = context;

    if (port == NULL || port->mutex == NULL) return DISPLAY_ERROR_RESOURCE;
    return osal_mutex_take(port->mutex, timeout_ms) == OSAL_SUCCESS
         ? DISPLAY_OK : DISPLAY_ERROR_TIMEOUT;
}

static display_status_t display_unlock(void *context)
{
    display_port_context_t *port = context;

    if (port == NULL || port->mutex == NULL) return DISPLAY_ERROR_RESOURCE;
    return osal_mutex_give(port->mutex) == OSAL_SUCCESS
         ? DISPLAY_OK : DISPLAY_ERROR_RESOURCE;
}

static display_status_t display_write(void *context, const uint8_t *data,
                                      size_t size, uint32_t timeout_ms)
{
    display_port_context_t *port = context;

    if (port == NULL || data == NULL || size == 0U || size > UINT16_MAX)
        return DISPLAY_ERROR_PARAMETER;
    return display_status_from_hal(HAL_SPI_Transmit(
        port->config.spi, (uint8_t *)data, (uint16_t)size, timeout_ms));
}

static display_status_t display_write_async(void *context,
                                            const uint8_t *data, size_t size)
{
    display_port_context_t *port = context;

    if (port == NULL || data == NULL || size == 0U || size > UINT16_MAX)
        return DISPLAY_ERROR_PARAMETER;
    return display_status_from_hal(HAL_SPI_Transmit_DMA(
        port->config.spi, (uint8_t *)data, (uint16_t)size));
}

static display_status_t display_wait_complete(void *context,
                                               uint32_t timeout_ms)
{
    display_port_context_t *port = context;
    uint32_t start;

    if (port == NULL) return DISPLAY_ERROR_PARAMETER;
    start = HAL_GetTick();

    while (HAL_SPI_GetState(port->config.spi) != HAL_SPI_STATE_READY)
    {
        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
        {
            (void)HAL_SPI_Abort(port->config.spi);
            return DISPLAY_ERROR_TIMEOUT;
        }
        osal_task_delay_ms(1U);
    }

    return HAL_SPI_GetError(port->config.spi) == HAL_SPI_ERROR_NONE
         ? DISPLAY_OK : DISPLAY_ERROR;
}

static void display_delay(void *context, uint32_t milliseconds)
{
    (void)context;
    osal_task_delay_ms(milliseconds);
}

static bool display_port_init(display_drv_t *dev)
{
    display_port_context_t *port = dev != NULL ? dev->user_data : NULL;
    GPIO_InitTypeDef gpio = {0};

    if (port == NULL) return false;
    if (port->mutex == NULL &&
        osal_mutex_create(&port->mutex) != OSAL_SUCCESS)
        return false;

    enable_gpio_clock(port->config.cs_port);
    enable_gpio_clock(port->config.dc_port);
    enable_gpio_clock(port->config.reset_port);
    if (port->config.bl_port != NULL) enable_gpio_clock(port->config.bl_port);

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Pin = port->config.cs_pin;
    HAL_GPIO_Init(port->config.cs_port, &gpio);
    gpio.Pin = port->config.dc_pin;
    HAL_GPIO_Init(port->config.dc_port, &gpio);
    gpio.Pin = port->config.reset_pin;
    HAL_GPIO_Init(port->config.reset_port, &gpio);
    if (port->config.bl_port != NULL && port->config.bl_pin != 0U)
    {
        gpio.Pin = port->config.bl_pin;
        HAL_GPIO_Init(port->config.bl_port, &gpio);
    }

    port->spi_interface = (display_spi_interface_t){
        .bus_context = port,
        .pf_spi_write = display_write,
        .pf_spi_write_dma = display_write_async,
        .pf_spi_wait_complete = display_wait_complete,
        .pf_lock = display_lock,
        .pf_unlock = display_unlock,
    };
    port->control_interface = (display_control_interface_t){
        .context = port,
        .pf_set_cs = display_cs,
        .pf_set_dc = display_dc,
        .pf_set_reset = display_reset,
        .pf_set_backlight = display_backlight,
    };
    port->delay_interface = (display_delay_interface_t){
        .context = port,
        .pf_delay_ms = display_delay,
    };

    return display_handler_init(&port->handler, &port->spi_interface,
                                &port->control_interface,
                                &port->delay_interface) == DISPLAY_OK;
}

static bool display_port_set_window(display_drv_t *dev,
                                    uint16_t x0, uint16_t y0,
                                    uint16_t x1, uint16_t y1)
{
    display_port_context_t *port = dev != NULL ? dev->user_data : NULL;
    return port != NULL && display_handler_set_window(
        &port->handler, x0, y0, x1, y1) == DISPLAY_OK;
}

static bool display_port_write_pixels(display_drv_t *dev,
                                      const uint8_t *pixels, size_t size)
{
    display_port_context_t *port = dev != NULL ? dev->user_data : NULL;
    return port != NULL && display_handler_write_pixels(
        &port->handler, pixels, size) == DISPLAY_OK;
}

static bool display_port_fill(display_drv_t *dev, uint16_t color)
{
    display_port_context_t *port = dev != NULL ? dev->user_data : NULL;
    return port != NULL &&
           display_handler_fill(&port->handler, color) == DISPLAY_OK;
}

static bool display_port_get_info(display_drv_t *dev,
                                  drv_adapter_display_info_t *info)
{
    display_port_context_t *port = dev != NULL ? dev->user_data : NULL;
    display_info_t bsp_info;

    if (port == NULL || info == NULL ||
        display_handler_get_info(&port->handler, &bsp_info) != DISPLAY_OK)
        return false;

    *info = (drv_adapter_display_info_t){
        .width = bsp_info.width,
        .height = bsp_info.height,
        .x_offset = bsp_info.x_offset,
        .y_offset = bsp_info.y_offset,
        .bits_per_pixel = bsp_info.bits_per_pixel,
        .requires_byte_swap = bsp_info.requires_byte_swap,
    };
    return true;
}

static bool display_port_sleep(display_drv_t *dev)
{
    display_port_context_t *port = dev != NULL ? dev->user_data : NULL;
    return port != NULL &&
           display_handler_sleep(&port->handler) == DISPLAY_OK;
}

static bool display_port_wakeup(display_drv_t *dev)
{
    display_port_context_t *port = dev != NULL ? dev->user_data : NULL;
    return port != NULL &&
           display_handler_wakeup(&port->handler) == DISPLAY_OK;
}

bool drv_adapter_port_display_register(
    uint32_t index, const display_port_config_t *config)
{
    display_port_context_t *port;
    display_drv_t driver;

    if (index >= DISPLAY_DEV_MAX || config == NULL || config->spi == NULL ||
        config->cs_port == NULL || config->cs_pin == 0U ||
        config->dc_port == NULL || config->dc_pin == 0U ||
        config->reset_port == NULL || config->reset_pin == 0U ||
        s_port[index].registered)
        return false;

    port = &s_port[index];
    port->config = *config;
    port->mutex = NULL;
    port->handler.initialized = false;
    driver = (display_drv_t){
        .idx = index,
        .user_data = port,
        .init = display_port_init,
        .set_window = display_port_set_window,
        .write_pixels = display_port_write_pixels,
        .fill = display_port_fill,
        .get_info = display_port_get_info,
        .sleep = display_port_sleep,
        .wakeup = display_port_wakeup,
    };

    if (!drv_adapter_display_reg(index, &driver)) return false;
    port->registered = true;
    return true;
}
