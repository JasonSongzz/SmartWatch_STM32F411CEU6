#include "drv_adapter_port_display.h"

#include "delay.h"
#include "osal.h"

typedef struct
{
    display_port_config_t config;
    osal_mutex_handle_t mutex;
} display_port_context_t;

static display_port_context_t s_display_context;
static bsp_spi_interface_t s_spi_interface;
static st7789t3_control_interface_t s_control_interface;
static bsp_delay_interface_t s_delay_interface;

static void display_cs(void *context, bool high)
{
    display_port_context_t *port = (display_port_context_t *)context;
    HAL_GPIO_WritePin(port->config.cs_port, port->config.cs_pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void display_dc(void *context, bool data_mode)
{
    display_port_context_t *port = (display_port_context_t *)context;
    HAL_GPIO_WritePin(port->config.dc_port, port->config.dc_pin,
                      data_mode ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void display_reset(void *context, bool high)
{
    display_port_context_t *port = (display_port_context_t *)context;
    HAL_GPIO_WritePin(port->config.reset_port, port->config.reset_pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void display_backlight(void *context, bool on)
{
    display_port_context_t *port = (display_port_context_t *)context;

    if (port->config.bl_port != NULL)
        HAL_GPIO_WritePin(port->config.bl_port, port->config.bl_pin,
                          on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bsp_io_status_t display_lock(void *context, uint32_t timeout_ms)
{
    display_port_context_t *port = (display_port_context_t *)context;

    if (port == NULL || port->mutex == NULL) return BSP_IO_ERROR;
    return osal_mutex_take(port->mutex, timeout_ms) == OSAL_SUCCESS
         ? BSP_IO_OK : BSP_IO_TIMEOUT;
}

static void display_unlock(void *context)
{
    display_port_context_t *port = (display_port_context_t *)context;

    if (port != NULL && port->mutex != NULL)
        (void)osal_mutex_give(port->mutex);
}

static bsp_io_status_t display_write(void *context, const uint8_t *data,
                                     size_t size, uint32_t timeout_ms)
{
    display_port_context_t *port = (display_port_context_t *)context;

    if (port == NULL || data == NULL || size == 0U || size > UINT16_MAX)
        return BSP_IO_ERROR;

    return HAL_SPI_Transmit(port->config.spi, (uint8_t *)data,
                            (uint16_t)size, timeout_ms) == HAL_OK
         ? BSP_IO_OK : BSP_IO_ERROR;
}

static bsp_io_status_t display_write_async(void *context,
                                           const uint8_t *data, size_t size)
{
    display_port_context_t *port = (display_port_context_t *)context;

    if (port == NULL || data == NULL || size == 0U || size > UINT16_MAX)
        return BSP_IO_ERROR;

    return HAL_SPI_Transmit_DMA(port->config.spi, (uint8_t *)data,
                                (uint16_t)size) == HAL_OK
         ? BSP_IO_OK : BSP_IO_ERROR;
}

static bsp_io_status_t display_wait_complete(void *context, uint32_t timeout_ms)
{
    display_port_context_t *port = (display_port_context_t *)context;
    uint32_t start;

    if (port == NULL) return BSP_IO_ERROR;
    start = HAL_GetTick();

    while (HAL_SPI_GetState(port->config.spi) != HAL_SPI_STATE_READY)
    {
        if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
        {
            (void)HAL_SPI_Abort(port->config.spi);
            return BSP_IO_TIMEOUT;
        }
        delay_ms(1U);
    }

    return HAL_SPI_GetError(port->config.spi) == HAL_SPI_ERROR_NONE
         ? BSP_IO_OK : BSP_IO_ERROR;
}

static void display_delay(void *context, uint32_t milliseconds)
{
    (void)context;
    delay_ms(milliseconds);
}

st7789t3_status_t drv_adapter_port_display_register(
    display_drv_t *display, const display_port_config_t *config)
{
    GPIO_InitTypeDef gpio = {0};

    if (display == NULL || config == NULL || config->spi == NULL ||
        config->cs_port == NULL || config->dc_port == NULL ||
        config->reset_port == NULL)
        return ST7789T3_ERROR_PARAM;

    s_display_context.config = *config;
    if (s_display_context.mutex == NULL &&
        osal_mutex_create(&s_display_context.mutex) != OSAL_SUCCESS)
        return ST7789T3_ERROR_RESOURCE;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Pin = config->cs_pin;
    HAL_GPIO_Init(config->cs_port, &gpio);
    gpio.Pin = config->dc_pin;
    HAL_GPIO_Init(config->dc_port, &gpio);
    gpio.Pin = config->reset_pin;
    HAL_GPIO_Init(config->reset_port, &gpio);

    if (config->bl_port != NULL)
    {
        gpio.Pin = config->bl_pin;
        HAL_GPIO_Init(config->bl_port, &gpio);
    }

    s_spi_interface = (bsp_spi_interface_t){
        .context = &s_display_context,
        .lock = display_lock,
        .unlock = display_unlock,
        .write = display_write,
        .write_async = display_write_async,
        .wait_complete = display_wait_complete,
    };
    s_control_interface = (st7789t3_control_interface_t){
        .context = &s_display_context,
        .set_cs = display_cs,
        .set_dc = display_dc,
        .set_reset = display_reset,
        .set_backlight = display_backlight,
    };
    s_delay_interface = (bsp_delay_interface_t){
        .context = &s_display_context,
        .delay_ms = display_delay,
    };

    return drv_adapter_display_init(display, &s_spi_interface,
                                    &s_control_interface, &s_delay_interface);
}
