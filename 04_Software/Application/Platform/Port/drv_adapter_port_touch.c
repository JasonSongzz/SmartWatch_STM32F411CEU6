#include "drv_adapter_port_touch.h"

#include "bsp_touch_handler.h"
#include "iic_hal.h"
#include "osal.h"

typedef struct
{
    bool registered;
    touch_port_config_t config;
    iic_bus_t iic_bus;
    osal_mutex_handle_t bus_mutex;
    touch_iic_interface_t iic_interface;
    touch_control_interface_t control_interface;
    touch_yield_interface_t yield_interface;
    bsp_touch_handler_t handler;
} touch_port_t;

static touch_port_t s_port[TOUCH_DEV_MAX];

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

static void touch_reset(void *context, bool high)
{
    touch_port_t *port = (touch_port_t *)context;

    HAL_GPIO_WritePin(port->config.reset_port, port->config.reset_pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool touch_interrupt_asserted(void *context)
{
    touch_port_t *port = (touch_port_t *)context;
    GPIO_PinState state;

    if (port->config.interrupt_port == NULL ||
        port->config.interrupt_pin == 0U)
        return true;

    state = HAL_GPIO_ReadPin(port->config.interrupt_port,
                            port->config.interrupt_pin);
    return port->config.interrupt_active_low
         ? state == GPIO_PIN_RESET : state == GPIO_PIN_SET;
}

static touch_status_t touch_i2c_init(void *context)
{
    touch_port_t *port = (touch_port_t *)context;

    if (port == NULL) return TOUCH_ERROR_PARAMETER;
    enable_gpio_clock(port->iic_bus.IIC_SDA_PORT);
    enable_gpio_clock(port->iic_bus.IIC_SCL_PORT);
    IICInit(&port->iic_bus);

    return TOUCH_OK;
}

static touch_status_t touch_i2c_lock(void *context, uint32_t timeout_ms)
{
    touch_port_t *port = (touch_port_t *)context;

    if (port == NULL || port->bus_mutex == NULL)
        return TOUCH_ERROR_RESOURCE;

    return osal_mutex_take(port->bus_mutex, timeout_ms) == OSAL_SUCCESS
         ? TOUCH_OK : TOUCH_ERROR_TIMEOUT;
}

static touch_status_t touch_i2c_unlock(void *context)
{
    touch_port_t *port = (touch_port_t *)context;

    if (port == NULL || port->bus_mutex == NULL)
        return TOUCH_ERROR_RESOURCE;

    return osal_mutex_give(port->bus_mutex) == OSAL_SUCCESS
         ? TOUCH_OK : TOUCH_ERROR_RESOURCE;
}

static touch_status_t touch_i2c_start(void *context)
{
    IICStart(&((touch_port_t *)context)->iic_bus);
    return TOUCH_OK;
}

static touch_status_t touch_i2c_stop(void *context)
{
    IICStop(&((touch_port_t *)context)->iic_bus);
    return TOUCH_OK;
}

static touch_status_t touch_i2c_wait_ack(void *context)
{
    return IICWaitAck(&((touch_port_t *)context)->iic_bus) == SUCCESS
         ? TOUCH_OK : TOUCH_ERROR_TIMEOUT;
}

static touch_status_t touch_i2c_send_ack(void *context)
{
    IICSendAck(&((touch_port_t *)context)->iic_bus);
    return TOUCH_OK;
}

static touch_status_t touch_i2c_send_no_ack(void *context)
{
    IICSendNotAck(&((touch_port_t *)context)->iic_bus);
    return TOUCH_OK;
}

static touch_status_t touch_i2c_send_byte(void *context, uint8_t data)
{
    IICSendByte(&((touch_port_t *)context)->iic_bus, data);
    return TOUCH_OK;
}

static touch_status_t touch_i2c_receive_byte(void *context, uint8_t *data)
{
    if (context == NULL || data == NULL) return TOUCH_ERROR_PARAMETER;

    *data = IICReceiveByte(&((touch_port_t *)context)->iic_bus);
    return TOUCH_OK;
}

static bool touch_port_init(touch_drv_t *dev)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    GPIO_InitTypeDef gpio = {0};

    if (port == NULL) return false;

    if (port->bus_mutex == NULL &&
        osal_mutex_create(&port->bus_mutex) != OSAL_SUCCESS)
        return false;

    enable_gpio_clock(port->config.reset_port);
    gpio.Pin = port->config.reset_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port->config.reset_port, &gpio);

    if (port->config.interrupt_port != NULL &&
        port->config.interrupt_pin != 0U)
    {
        enable_gpio_clock(port->config.interrupt_port);
        gpio.Pin = port->config.interrupt_pin;
        gpio.Mode = GPIO_MODE_INPUT;
        gpio.Pull = port->config.interrupt_active_low
                  ? GPIO_PULLUP : GPIO_PULLDOWN;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(port->config.interrupt_port, &gpio);
    }

    port->iic_interface = (touch_iic_interface_t){
        .bus_context = port,
        .pf_iic_init = touch_i2c_init,
        .pf_iic_deinit = NULL,
        .pf_iic_start = touch_i2c_start,
        .pf_iic_stop = touch_i2c_stop,
        .pf_iic_wait_ack = touch_i2c_wait_ack,
        .pf_iic_send_ack = touch_i2c_send_ack,
        .pf_iic_send_no_ack = touch_i2c_send_no_ack,
        .pf_iic_send_byte = touch_i2c_send_byte,
        .pf_iic_receive_byte = touch_i2c_receive_byte,
        .pf_lock = touch_i2c_lock,
        .pf_unlock = touch_i2c_unlock,
    };
    port->control_interface = (touch_control_interface_t){
        .context = port,
        .pf_set_reset = touch_reset,
        .pf_is_interrupt_asserted = touch_interrupt_asserted,
    };
    port->yield_interface.pf_rtos_yield = osal_task_delay_ms;

    return touch_handler_init(&port->handler, &port->iic_interface,
                              &port->control_interface,
                              &port->yield_interface) == TOUCH_OK;
}

static drv_adapter_touch_status_t touch_port_read(
    touch_drv_t *dev, drv_adapter_touch_point_t *point)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    touch_point_t raw_point;
    touch_status_t status;

    if (port == NULL || point == NULL) return DRV_ADAPTER_TOUCH_ERROR;

    status = touch_handler_read(&port->handler, &raw_point);
    if (status == TOUCH_NO_TOUCH)
    {
        *point = (drv_adapter_touch_point_t){0};
        return DRV_ADAPTER_TOUCH_NO_TOUCH;
    }
    if (status != TOUCH_OK) return DRV_ADAPTER_TOUCH_ERROR;

    point->x = raw_point.x;
    point->y = raw_point.y;
    point->gesture = raw_point.gesture;
    point->event = raw_point.event;
    point->fingers = raw_point.fingers;

    return DRV_ADAPTER_TOUCH_OK;
}

static bool touch_port_sleep(touch_drv_t *dev)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;

    return port != NULL &&
           touch_handler_sleep(&port->handler) == TOUCH_OK;
}

static bool touch_port_get_info(touch_drv_t *dev,
                                drv_adapter_touch_info_t *info)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    touch_info_t bsp_info;

    if (port == NULL || info == NULL ||
        touch_handler_get_info(&port->handler, &bsp_info) != TOUCH_OK)
        return false;

    *info = (drv_adapter_touch_info_t){
        .width = bsp_info.width,
        .height = bsp_info.height,
        .max_points = bsp_info.max_points,
    };
    return true;
}

static bool touch_port_wakeup(touch_drv_t *dev)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;

    return port != NULL &&
           touch_handler_wakeup(&port->handler) == TOUCH_OK;
}

bool drv_adapter_port_touch_register(uint32_t index,
                                     const touch_port_config_t *config)
{
    touch_port_t *port;
    touch_drv_t driver;
    const iic_bus_t *bus;
    const touch_port_config_t *selected_config;
    const iic_bus_t default_bus = {
        .IIC_SDA_PORT = TOUCH_IIC_SDA_PORT,
        .IIC_SCL_PORT = TOUCH_IIC_SCL_PORT,
        .IIC_SDA_PIN = TOUCH_IIC_SDA_PIN,
        .IIC_SCL_PIN = TOUCH_IIC_SCL_PIN,
    };
    const touch_port_config_t default_config = {
        .iic_bus = (void *)&default_bus,
        .bus_mutex = NULL,
        .reset_port = TOUCH_RESET_PORT,
        .reset_pin = TOUCH_RESET_PIN,
        .interrupt_port = TOUCH_INTERRUPT_PORT,
        .interrupt_pin = TOUCH_INTERRUPT_PIN,
        .interrupt_active_low = true,
    };

    if (index >= TOUCH_DEV_MAX || s_port[index].registered)
        return false;

    selected_config = config != NULL ? config : &default_config;
    if (selected_config->iic_bus == NULL ||
        selected_config->reset_port == NULL ||
        selected_config->reset_pin == 0U)
        return false;

    bus = (const iic_bus_t *)selected_config->iic_bus;
    if (bus->IIC_SDA_PORT == NULL || bus->IIC_SCL_PORT == NULL ||
        bus->IIC_SDA_PIN == 0U || bus->IIC_SCL_PIN == 0U)
        return false;

    port = &s_port[index];
    port->config = *selected_config;
    port->iic_bus = *bus;
    port->config.iic_bus = &port->iic_bus;
    port->bus_mutex = selected_config->bus_mutex;
    port->handler.initialized = false;

    driver = (touch_drv_t){
        .idx = index,
        .user_data = port,
        .init = touch_port_init,
        .read = touch_port_read,
        .get_info = touch_port_get_info,
        .sleep = touch_port_sleep,
        .wakeup = touch_port_wakeup,
    };

    if (!drv_adapter_touch_reg(index, &driver)) return false;

    port->registered = true;
    return true;
}
