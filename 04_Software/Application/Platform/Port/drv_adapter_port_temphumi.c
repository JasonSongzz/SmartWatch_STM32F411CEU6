#include "drv_adapter_port_temphumi.h"

#include "bsp_temp_humi_handler.h"
#include "iic_hal.h"
#include "osal.h"

#define TEMPHUMI_MUTEX_TIMEOUT_MS (100U)

typedef struct
{
    bool registered;
    float temperature;
    float humidity;
    float temp_scale;
    iic_bus_t iic_bus;
    osal_mutex_handle_t bus_mutex;
    osal_mutex_handle_t data_mutex;
    temp_humi_iic_interface_t iic_interface;
    temp_humi_yield_interface_t yield_interface;
    bsp_temp_humi_handler_t handler;
} temphumi_port_t;

static temphumi_port_t s_port[TEMP_HUMI_DEV_MAX];

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

static temp_humi_status_t temphumi_iic_init(void *context)
{
    temphumi_port_t *port = (temphumi_port_t *)context;

    if (port == NULL) return TEMP_HUMI_ERROR_PARAMETER;
    enable_gpio_clock(port->iic_bus.IIC_SDA_PORT);
    enable_gpio_clock(port->iic_bus.IIC_SCL_PORT);
    IICInit(&port->iic_bus);

    return TEMP_HUMI_OK;
}

static temp_humi_status_t temphumi_iic_lock(void *context,
                                            uint32_t timeout_ms)
{
    temphumi_port_t *port = (temphumi_port_t *)context;

    if (port == NULL || port->bus_mutex == NULL)
        return TEMP_HUMI_ERROR_RESOURCE;

    return osal_mutex_take(port->bus_mutex, timeout_ms) == OSAL_SUCCESS
         ? TEMP_HUMI_OK : TEMP_HUMI_ERROR_TIMEOUT;
}

static temp_humi_status_t temphumi_iic_unlock(void *context)
{
    temphumi_port_t *port = (temphumi_port_t *)context;

    if (port == NULL || port->bus_mutex == NULL)
        return TEMP_HUMI_ERROR_RESOURCE;

    return osal_mutex_give(port->bus_mutex) == OSAL_SUCCESS
         ? TEMP_HUMI_OK : TEMP_HUMI_ERROR_RESOURCE;
}

static temp_humi_status_t temphumi_iic_start(void *context)
{
    IICStart(&((temphumi_port_t *)context)->iic_bus);
    return TEMP_HUMI_OK;
}

static temp_humi_status_t temphumi_iic_stop(void *context)
{
    IICStop(&((temphumi_port_t *)context)->iic_bus);
    return TEMP_HUMI_OK;
}

static temp_humi_status_t temphumi_iic_wait_ack(void *context)
{
    return IICWaitAck(&((temphumi_port_t *)context)->iic_bus) == SUCCESS
         ? TEMP_HUMI_OK : TEMP_HUMI_ERROR_TIMEOUT;
}

static temp_humi_status_t temphumi_iic_send_ack(void *context)
{
    IICSendAck(&((temphumi_port_t *)context)->iic_bus);
    return TEMP_HUMI_OK;
}

static temp_humi_status_t temphumi_iic_send_no_ack(void *context)
{
    IICSendNotAck(&((temphumi_port_t *)context)->iic_bus);
    return TEMP_HUMI_OK;
}

static temp_humi_status_t temphumi_iic_send_byte(void *context, uint8_t data)
{
    IICSendByte(&((temphumi_port_t *)context)->iic_bus, data);
    return TEMP_HUMI_OK;
}

static temp_humi_status_t temphumi_iic_receive_byte(void *context,
                                                     uint8_t *data)
{
    if (context == NULL || data == NULL)
        return TEMP_HUMI_ERROR_PARAMETER;

    *data = IICReceiveByte(&((temphumi_port_t *)context)->iic_bus);
    return TEMP_HUMI_OK;
}

static bool temphumi_port_init(temphumi_drv_t *dev)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;

    if (port == NULL) return false;

    if (port->bus_mutex == NULL &&
        osal_mutex_create(&port->bus_mutex) != OSAL_SUCCESS)
        return false;

    if (port->data_mutex == NULL &&
        osal_mutex_create(&port->data_mutex) != OSAL_SUCCESS)
        return false;

    port->iic_interface = (temp_humi_iic_interface_t){
        .bus_context = port,
        .pf_iic_init = temphumi_iic_init,
        .pf_iic_deinit = NULL,
        .pf_iic_start = temphumi_iic_start,
        .pf_iic_stop = temphumi_iic_stop,
        .pf_iic_wait_ack = temphumi_iic_wait_ack,
        .pf_iic_send_ack = temphumi_iic_send_ack,
        .pf_iic_send_no_ack = temphumi_iic_send_no_ack,
        .pf_iic_send_byte = temphumi_iic_send_byte,
        .pf_iic_receive_byte = temphumi_iic_receive_byte,
        .pf_lock = temphumi_iic_lock,
        .pf_unlock = temphumi_iic_unlock,
    };
    port->yield_interface.pf_rtos_yield = osal_task_delay_ms;

    return temp_humi_handler_init(&port->handler, &port->iic_interface,
                                  &port->yield_interface) == TEMP_HUMI_OK;
}

static bool temphumi_port_refresh(temphumi_drv_t *dev)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;
    float temperature;
    float humidity;

    if (port == NULL || temp_humi_handler_read(
            &port->handler, &temperature, &humidity) != TEMP_HUMI_OK)
        return false;

    if (osal_mutex_take(port->data_mutex,
                        TEMPHUMI_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    port->temperature = temperature * port->temp_scale;
    port->humidity = humidity;

    return osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
}

static bool temphumi_port_read_cached(temphumi_drv_t *dev,
                                      float *temperature, float *humidity)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;

    if (port == NULL || temperature == NULL || humidity == NULL ||
        port->data_mutex == NULL)
        return false;

    if (osal_mutex_take(port->data_mutex,
                        TEMPHUMI_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    *temperature = port->temperature;
    *humidity = port->humidity;

    return osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
}

static bool temphumi_port_sleep(temphumi_drv_t *dev)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;

    return port != NULL &&
           temp_humi_handler_sleep(&port->handler) == TEMP_HUMI_OK;
}

static bool temphumi_port_wakeup(temphumi_drv_t *dev)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;

    return port != NULL &&
           temp_humi_handler_wakeup(&port->handler) == TEMP_HUMI_OK;
}

bool drv_adapter_port_temphumi_register(
    uint32_t index, const temphumi_port_config_t *config)
{
    temphumi_drv_t driver;
    temphumi_port_t *port;
    const iic_bus_t *bus;
    const iic_bus_t default_bus = {
        .IIC_SDA_PORT = TEMP_HUMI_IIC_SDA_PORT,
        .IIC_SCL_PORT = TEMP_HUMI_IIC_SCL_PORT,
        .IIC_SDA_PIN = TEMP_HUMI_IIC_SDA_PIN,
        .IIC_SCL_PIN = TEMP_HUMI_IIC_SCL_PIN,
    };

    if (index >= TEMP_HUMI_DEV_MAX || s_port[index].registered)
        return false;

    bus = config != NULL && config->iic_bus != NULL
        ? (const iic_bus_t *)config->iic_bus : &default_bus;
    if (bus->IIC_SDA_PORT == NULL || bus->IIC_SCL_PORT == NULL ||
        bus->IIC_SDA_PIN == 0U || bus->IIC_SCL_PIN == 0U)
        return false;

    port = &s_port[index];
    port->iic_bus = *bus;
    port->bus_mutex = config != NULL ? config->bus_mutex : NULL;
    port->data_mutex = NULL;
    port->temp_scale = config != NULL && config->temp_scale > 0.0f
                     ? config->temp_scale : 1.0f;
    port->temperature = 0.0f;
    port->humidity = 0.0f;
    port->handler.initialized = false;

    driver = (temphumi_drv_t){
        .idx = index,
        .user_data = port,
        .init = temphumi_port_init,
        .refresh = temphumi_port_refresh,
        .read_cached = temphumi_port_read_cached,
        .sleep = temphumi_port_sleep,
        .wakeup = temphumi_port_wakeup,
    };

    if (!drv_adapter_temphumi_reg(index, &driver)) return false;

    port->registered = true;
    return true;
}
