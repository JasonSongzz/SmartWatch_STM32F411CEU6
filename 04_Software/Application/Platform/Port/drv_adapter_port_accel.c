#include "drv_adapter_port_accel.h"

#include "bsp_accel_xxx_handler.h"
#include "iic_hal.h"
#include "osal.h"

#define ACCEL_DATA_MUTEX_TIMEOUT_MS (100U)

typedef struct
{
    bool registered;
    mpu6050_accel_t accel;
    float accel_scale;
    iic_bus_t iic_bus;
    osal_mutex_handle_t data_mutex;
    mpu6050_iic_driver_interface_t iic_interface;
    mpu6050_yield_interface_t yield_interface;
    bsp_accel_handler_t handler;
} accel_port_t;

static accel_port_t s_port[ACCEL_DEV_MAX];

static mpu6050_status_t accel_iic_init(void *bus)
{
    IICInit((iic_bus_t *)bus);

    return MPU6050_OK;
}

static mpu6050_status_t accel_iic_start(void *bus)
{
    IICStart((iic_bus_t *)bus);

    return MPU6050_OK;
}

static mpu6050_status_t accel_iic_stop(void *bus)
{
    IICStop((iic_bus_t *)bus);

    return MPU6050_OK;
}

static mpu6050_status_t accel_iic_wait_ack(void *bus)
{
    return IICWaitAck((iic_bus_t *)bus) == SUCCESS ? MPU6050_OK : MPU6050_ERRORTIMEOUT;
}

static mpu6050_status_t accel_iic_send_ack(void *bus)
{
    IICSendAck((iic_bus_t *)bus);

    return MPU6050_OK;
}

static mpu6050_status_t accel_iic_send_no_ack(void *bus)
{
    IICSendNotAck((iic_bus_t *)bus);

    return MPU6050_OK;
}

static mpu6050_status_t accel_iic_send_byte(void *bus, uint8_t data)
{
    IICSendByte((iic_bus_t *)bus, data);

    return MPU6050_OK;
}

static mpu6050_status_t accel_iic_receive_byte(void *bus, uint8_t *data)
{
    if (data == NULL) return MPU6050_ERRORPARAMETER;

    *data = IICReceiveByte((iic_bus_t *)bus);

    return MPU6050_OK;
}

static mpu6050_status_t accel_iic_critical_enter(void)
{
    osal_enter_critical();

    return MPU6050_OK;
}

static mpu6050_status_t accel_iic_critical_exit(void)
{
    osal_exit_critical();

    return MPU6050_OK;
}

static bool accel_port_init(accel_drv_t *dev)
{
    accel_port_t *port = (accel_port_t *)dev->user_data;

    if (port == NULL) return false;

    if (port->data_mutex == NULL &&
        osal_mutex_create(&port->data_mutex) != OSAL_SUCCESS)

        return false;

#ifndef HARDWARE_IIC
    port->iic_interface = (mpu6050_iic_driver_interface_t){
        .bus_context = &port->iic_bus,
        .pf_iic_init = accel_iic_init,
        .pf_iic_start = accel_iic_start,
        .pf_iic_stop = accel_iic_stop,
        .pf_iic_wait_ack = accel_iic_wait_ack,
        .pf_iic_send_ack = accel_iic_send_ack,
        .pf_iic_send_no_ack = accel_iic_send_no_ack,
        .pf_iic_send_byte = accel_iic_send_byte,
        .pf_iic_receive_byte = accel_iic_receive_byte,
        .pf_critical_enter = accel_iic_critical_enter,
        .pf_critical_exit = accel_iic_critical_exit,
    };
#else
    /* Fill pf_iic_write/pf_iic_read with the selected hardware-I2C adapter. */
    return false;
#endif

    port->yield_interface.pf_rtos_yield = osal_task_delay_ms;
    
    return accel_handler_init(&port->handler, &port->iic_interface,
                              &port->yield_interface) == MPU6050_OK;
}

static bool accel_port_refresh(accel_drv_t *dev)
{
    accel_port_t *port = (accel_port_t *)dev->user_data;
    mpu6050_accel_t accel;

    if (port == NULL || accel_handler_read(&port->handler, &accel) != MPU6050_OK)
        return false;

    if (osal_mutex_take(port->data_mutex, ACCEL_DATA_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    port->accel.x = accel.x * port->accel_scale;
    port->accel.y = accel.y * port->accel_scale;
    port->accel.z = accel.z * port->accel_scale;
    (void)osal_mutex_give(port->data_mutex);

    return true;
}

static void accel_port_read_cached(accel_drv_t *dev, float *x, float *y, float *z)
{
    accel_port_t *port = (accel_port_t *)dev->user_data;

    if (port == NULL || x == NULL || y == NULL || z == NULL) return;

    if (port->data_mutex != NULL &&
        osal_mutex_take(port->data_mutex, ACCEL_DATA_MUTEX_TIMEOUT_MS) == OSAL_SUCCESS)
    {
        *x = port->accel.x;
        *y = port->accel.y;
        *z = port->accel.z;
        (void)osal_mutex_give(port->data_mutex);
    }
    else
    {
        *x = port->accel.x;
        *y = port->accel.y;
        *z = port->accel.z;
    }
}

static bool accel_port_sleep(accel_drv_t *dev)
{
    accel_port_t *port = (accel_port_t *)dev->user_data;

    return port != NULL && accel_handler_sleep(&port->handler) == MPU6050_OK;
}

static bool accel_port_wakeup(accel_drv_t *dev)
{
    accel_port_t *port = (accel_port_t *)dev->user_data;

    return port != NULL && accel_handler_wakeup(&port->handler) == MPU6050_OK;
}

bool drv_adapter_port_accel_register(uint32_t index,
                                     const accel_port_config_t *config)
{
    accel_drv_t driver;
    accel_port_t *port;

    if (index >= ACCEL_DEV_MAX || s_port[index].registered) return false;

    port = &s_port[index];

    if (config == NULL || config->iic_bus == NULL) return false;

    port->iic_bus = *(iic_bus_t *)config->iic_bus;

    port->accel_scale = config != NULL && config->accel_scale > 0.0f
                     ? config->accel_scale : 1.0f;
    port->accel = (mpu6050_accel_t){0.0f, 0.0f, 0.0f};
    port->data_mutex = NULL;
    port->handler.initialized = false;

    driver.idx = index;
    driver.user_data = port;
    driver.init = accel_port_init;
    driver.refresh = accel_port_refresh;
    driver.read_cached = accel_port_read_cached;
    driver.sleep = accel_port_sleep;
    driver.wakeup = accel_port_wakeup;

    if (!drv_adapter_accel_reg(index, &driver)) return false;
    port->registered = true;

    return true;
}
