#include "drv_adapter_port_touch.h"

#include "delay.h"

typedef struct
{
    touch_port_config_t config;
} touch_port_context_t;

static touch_port_context_t s_touch_context;
static cst816t_iic_driver_interface_t s_i2c_interface;
static cst816t_control_interface_t s_control_interface;
static cst816t_delay_interface_t s_delay_interface;

static void enable_gpio_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
#ifdef GPIOD
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
#ifdef GPIOE
    else if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
#ifdef GPIOH
    else if (port == GPIOH) __HAL_RCC_GPIOH_CLK_ENABLE();
#endif
}

static void touch_reset(void *context, bool high)
{
    touch_port_context_t *port = (touch_port_context_t *)context;

    HAL_GPIO_WritePin(port->config.reset_port, port->config.reset_pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool touch_interrupt_asserted(void *context)
{
    touch_port_context_t *port = (touch_port_context_t *)context;
    GPIO_PinState state;

    if (port->config.interrupt_port == NULL || port->config.interrupt_pin == 0U)
        return true;

    state = HAL_GPIO_ReadPin(port->config.interrupt_port,
                            port->config.interrupt_pin);
    return port->config.interrupt_active_low
         ? state == GPIO_PIN_RESET : state == GPIO_PIN_SET;
}

static void touch_delay(void *context, uint32_t milliseconds)
{
    (void)context;
    delay_ms(milliseconds);
}

static cst816t_status_t touch_i2c_read(void *bus_context, uint8_t address,
                                       uint8_t reg, uint8_t *data,
                                       uint16_t size, uint32_t timeout_ms)
{
    return drv_adapter_port_soft_i2c_mem_read(
               (drv_soft_i2c_port_t *)bus_context, address, reg,
               data, size, timeout_ms)
         ? CST816T_OK : CST816T_ERROR;
}

static cst816t_status_t touch_i2c_write(void *bus_context, uint8_t address,
                                        uint8_t reg, const uint8_t *data,
                                        uint16_t size, uint32_t timeout_ms)
{
    return drv_adapter_port_soft_i2c_mem_write(
               (drv_soft_i2c_port_t *)bus_context, address, reg,
               data, size, timeout_ms)
         ? CST816T_OK : CST816T_ERROR;
}

static cst816t_status_t touch_i2c_lock(void *bus_context,
                                       uint32_t timeout_ms)
{
    return drv_adapter_port_soft_i2c_lock(
               (drv_soft_i2c_port_t *)bus_context, timeout_ms)
         ? CST816T_OK : CST816T_ERROR_TIMEOUT;
}

static cst816t_status_t touch_i2c_unlock(void *bus_context)
{
    drv_adapter_port_soft_i2c_unlock((drv_soft_i2c_port_t *)bus_context);
    return CST816T_OK;
}

cst816t_status_t drv_adapter_port_touch_register(
    touch_drv_t *touch, const touch_port_config_t *config,
    uint16_t width, uint16_t height)
{
    GPIO_InitTypeDef gpio = {0};

    if (touch == NULL || config == NULL || config->i2c_port == NULL ||
        !config->i2c_port->initialized ||
        config->reset_port == NULL || config->reset_pin == 0U ||
        width == 0U || height == 0U)
        return CST816T_ERROR_PARAM;

    s_touch_context.config = *config;
    enable_gpio_clock(config->reset_port);
    gpio.Pin = config->reset_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(config->reset_port, &gpio);

    if (config->interrupt_port != NULL && config->interrupt_pin != 0U)
    {
        enable_gpio_clock(config->interrupt_port);
        gpio.Pin = config->interrupt_pin;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = config->interrupt_active_low ? GPIO_PULLUP : GPIO_PULLDOWN;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(config->interrupt_port, &gpio);
    }

    s_i2c_interface = (cst816t_iic_driver_interface_t){
        .bus_context = config->i2c_port,
        .pf_iic_read = touch_i2c_read,
        .pf_iic_write = touch_i2c_write,
        .pf_lock = touch_i2c_lock,
        .pf_unlock = touch_i2c_unlock,
    };
    s_control_interface = (cst816t_control_interface_t){
        .context = &s_touch_context,
        .pf_set_reset = touch_reset,
        .pf_is_interrupt_asserted = touch_interrupt_asserted,
    };
    s_delay_interface = (cst816t_delay_interface_t){
        .context = &s_touch_context,
        .pf_delay_ms = touch_delay,
    };

    return drv_adapter_touch_init(touch, &s_i2c_interface,
                                  &s_control_interface, &s_delay_interface,
                                  width, height);
}
