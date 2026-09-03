#include "drv_adapter_port_accel.h"

#include "bsp_accel_handler.h"
#include "iic_hal.h"
#include "osal.h"

#define ACCEL_MUTEX_TIMEOUT_MS (100U)

typedef struct
{
    bool registered;
    accel_data_t accel;
    float accel_scale;
    iic_bus_t iic_bus;
    osal_mutex_handle_t bus_mutex;
    osal_mutex_handle_t data_mutex;
    accel_iic_interface_t iic_interface;
    accel_yield_interface_t yield_interface;
    bsp_accel_handler_t handler;
} accel_port_t;

static accel_port_t s_port[ACCEL_DEV_MAX];

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

static accel_status_t accel_iic_init(void *context)
{
    accel_port_t *port = (accel_port_t *)context;

    if (port == NULL) return ACCEL_ERROR_PARAMETER;
    enable_gpio_clock(port->iic_bus.IIC_SDA_PORT);
    enable_gpio_clock(port->iic_bus.IIC_SCL_PORT);
    IICInit(&port->iic_bus);

    return ACCEL_OK;
}

static accel_status_t accel_iic_lock(void *context, uint32_t timeout_ms)
{
    accel_port_t *port = (accel_port_t *)context;

    if (port == NULL || port->bus_mutex == NULL)
        return ACCEL_ERROR_RESOURCE;

    return osal_mutex_take(port->bus_mutex, timeout_ms) == OSAL_SUCCESS
         ? ACCEL_OK : ACCEL_ERROR_TIMEOUT;
}

static accel_status_t accel_iic_unlock(void *context)
{
    accel_port_t *port = (accel_port_t *)context;

    if (port == NULL || port->bus_mutex == NULL)
        return ACCEL_ERROR_RESOURCE;

    return osal_mutex_give(port->bus_mutex) == OSAL_SUCCESS
         ? ACCEL_OK : ACCEL_ERROR_RESOURCE;
}

static accel_status_t accel_iic_start(void *context)
{
    IICStart(&((accel_port_t *)context)->iic_bus);
    return ACCEL_OK;
}

static accel_status_t accel_iic_stop(void *context)
{
    IICStop(&((accel_port_t *)context)->iic_bus);
    return ACCEL_OK;
}

static accel_status_t accel_iic_wait_ack(void *context)
{
    return IICWaitAck(&((accel_port_t *)context)->iic_bus) == SUCCESS
         ? ACCEL_OK : ACCEL_ERROR_TIMEOUT;
}

static accel_status_t accel_iic_send_ack(void *context)
{
    IICSendAck(&((accel_port_t *)context)->iic_bus);
    return ACCEL_OK;
}

static accel_status_t accel_iic_send_no_ack(void *context)
{
    IICSendNotAck(&((accel_port_t *)context)->iic_bus);
    return ACCEL_OK;
}

static accel_status_t accel_iic_send_byte(void *context, uint8_t data)
{
    IICSendByte(&((accel_port_t *)context)->iic_bus, data);
    return ACCEL_OK;
}

static accel_status_t accel_iic_receive_byte(void *context, uint8_t *data)
{
    if (context == NULL || data == NULL) return ACCEL_ERROR_PARAMETER;

    *data = IICReceiveByte(&((accel_port_t *)context)->iic_bus);
    return ACCEL_OK;
}

static bool accel_port_init(accel_drv_t *dev)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;

    if (port == NULL) return false;

    if (port->bus_mutex == NULL &&
        osal_mutex_create(&port->bus_mutex) != OSAL_SUCCESS)
        return false;

    if (port->data_mutex == NULL &&
        osal_mutex_create(&port->data_mutex) != OSAL_SUCCESS)
        return false;

    port->iic_interface = (accel_iic_interface_t){
        .bus_context = port,
        .pf_iic_init = accel_iic_init,
        .pf_iic_deinit = NULL,
        .pf_iic_start = accel_iic_start,
        .pf_iic_stop = accel_iic_stop,
        .pf_iic_wait_ack = accel_iic_wait_ack,
        .pf_iic_send_ack = accel_iic_send_ack,
        .pf_iic_send_no_ack = accel_iic_send_no_ack,
        .pf_iic_send_byte = accel_iic_send_byte,
        .pf_iic_receive_byte = accel_iic_receive_byte,
        .pf_lock = accel_iic_lock,
        .pf_unlock = accel_iic_unlock,
    };
    port->yield_interface.pf_rtos_yield = osal_task_delay_ms;

    return accel_handler_init(&port->handler, &port->iic_interface,
                              &port->yield_interface) == ACCEL_OK;
}

static bool accel_port_refresh(accel_drv_t *dev)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;
    accel_data_t accel;

    if (port == NULL ||
        accel_handler_read(&port->handler, &accel) != ACCEL_OK)
        return false;

    if (osal_mutex_take(port->data_mutex,
                        ACCEL_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    port->accel.x = accel.x * port->accel_scale;
    port->accel.y = accel.y * port->accel_scale;
    port->accel.z = accel.z * port->accel_scale;

    return osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
}

static bool accel_port_read_cached(accel_drv_t *dev,
                                   float *x, float *y, float *z)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;

    if (port == NULL || x == NULL || y == NULL || z == NULL ||
        port->data_mutex == NULL)
        return false;

    if (osal_mutex_take(port->data_mutex,
                        ACCEL_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    *x = port->accel.x;
    *y = port->accel.y;
    *z = port->accel.z;

    return osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
}

static bool accel_port_sleep(accel_drv_t *dev)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;

    return port != NULL &&
           accel_handler_sleep(&port->handler) == ACCEL_OK;
}

static bool accel_port_wakeup(accel_drv_t *dev)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;

    return port != NULL &&
           accel_handler_wakeup(&port->handler) == ACCEL_OK;
}

bool drv_adapter_port_accel_register(uint32_t index,
                                     const accel_port_config_t *config)
{
    accel_drv_t driver;
    accel_port_t *port;
    const iic_bus_t *bus;
    const iic_bus_t default_bus = {
        .IIC_SDA_PORT = ACCEL_IIC_SDA_PORT,
        .IIC_SCL_PORT = ACCEL_IIC_SCL_PORT,
        .IIC_SDA_PIN = ACCEL_IIC_SDA_PIN,
        .IIC_SCL_PIN = ACCEL_IIC_SCL_PIN,
    };

    if (index >= ACCEL_DEV_MAX || s_port[index].registered)
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
    port->accel_scale = config != NULL && config->accel_scale > 0.0f
                      ? config->accel_scale : 1.0f;
    port->accel = (accel_data_t){0.0f, 0.0f, 0.0f};
    port->handler.initialized = false;

    driver = (accel_drv_t){
        .idx = index,
        .user_data = port,
        .init = accel_port_init,
        .refresh = accel_port_refresh,
        .read_cached = accel_port_read_cached,
        .sleep = accel_port_sleep,
        .wakeup = accel_port_wakeup,
    };

    if (!drv_adapter_accel_reg(index, &driver)) return false;

    port->registered = true;
    return true;
}
