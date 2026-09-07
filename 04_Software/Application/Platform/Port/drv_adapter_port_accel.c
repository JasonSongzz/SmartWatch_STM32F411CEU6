#include "drv_adapter_port_accel.h"

#include "bsp_accel_handler.h"
#include "iic_hal.h"
#include "osal.h"

#define ACCEL_MUTEX_TIMEOUT_MS (100U)

/**
 * @brief 单个加速度计设备的 Port 私有上下文。
 *
 * 锁职责：bus_mutex 只包围一次 I2C 事务；sample_mutex 串行化 Handler
 * 状态、校准、滤波和电源操作；data_mutex 只保护已发布快照。
 * 需要嵌套时固定按 sample_mutex -> data_mutex 获取，禁止反向加锁。
 */
typedef struct
{
    bool registered; /**< 是否已经占用对应 Wrapper 设备槽。 */
    bool data_valid; /**< snapshot 是否包含至少一次成功采样。 */
    accel_snapshot_t snapshot; /**< 供多个消费者读取的最近原子快照。 */
    iic_bus_t iic_bus; /**< 注册时复制的软件 I2C 引脚描述。 */
    osal_mutex_handle_t bus_mutex; /**< 完整 I2C 事务互斥锁。 */
    osal_mutex_handle_t sample_mutex; /**< Handler 可变状态和设备操作锁。 */
    osal_mutex_handle_t data_mutex; /**< snapshot 与 data_valid 发布锁。 */
    accel_iic_interface_t iic_interface; /**< 注入 BSP 的 I2C 适配表。 */
    accel_yield_interface_t yield_interface; /**< 注入 BSP 的 OSAL 延时适配。 */
    bsp_accel_handler_t handler; /**< 校准、滤波及具体驱动上下文。 */
} accel_port_t;

/** 每个已注册设备拥有独立上下文，索引与 Wrapper 设备索引一致。 */
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

    if (port->sample_mutex == NULL &&
        osal_mutex_create(&port->sample_mutex) != OSAL_SUCCESS)
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

    port->data_valid = false;
    return accel_handler_init(&port->handler, &port->iic_interface,
                              &port->yield_interface) == ACCEL_OK;
}

static bool accel_port_refresh(accel_drv_t *dev)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;
    accel_imu_data_t imu;
    bool result;

    if (port == NULL || port->sample_mutex == NULL ||
        osal_mutex_take(port->sample_mutex,
                        ACCEL_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    if (accel_handler_read_imu(&port->handler, &imu) != ACCEL_OK) {
        (void)osal_mutex_give(port->sample_mutex);
        return false;
    }

    if (osal_mutex_take(port->data_mutex,
                        ACCEL_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS) {
        (void)osal_mutex_give(port->sample_mutex);
        return false;
    }

    port->snapshot.accel_g.x = imu.accel_g.x;
    port->snapshot.accel_g.y = imu.accel_g.y;
    port->snapshot.accel_g.z = imu.accel_g.z;
    port->snapshot.gyro_dps.x = imu.gyro_dps.x;
    port->snapshot.gyro_dps.y = imu.gyro_dps.y;
    port->snapshot.gyro_dps.z = imu.gyro_dps.z;
    port->snapshot.temperature_c = imu.temperature_c;
    port->snapshot.timestamp_ms = osal_time_get_ms();
    ++port->snapshot.sequence;
    port->data_valid = true;

    result = osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool accel_port_read_snapshot(accel_drv_t *dev,
                                     accel_snapshot_t *snapshot)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;

    if (port == NULL || snapshot == NULL || port->data_mutex == NULL ||
        osal_mutex_take(port->data_mutex,
                        ACCEL_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    if (!port->data_valid) {
        (void)osal_mutex_give(port->data_mutex);
        return false;
    }

    *snapshot = port->snapshot;
    return osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
}

static bool accel_port_read_cached(accel_drv_t *dev,
                                   float *x, float *y, float *z)
{
    accel_snapshot_t snapshot;

    if (x == NULL || y == NULL || z == NULL ||
        !accel_port_read_snapshot(dev, &snapshot))
        return false;

    *x = snapshot.accel_g.x;
    *y = snapshot.accel_g.y;
    *z = snapshot.accel_g.z;
    return true;
}

static bool accel_port_invalidate_data(accel_port_t *port)
{
    bool result;

    if (port->data_mutex == NULL ||
        osal_mutex_take(port->data_mutex,
                        ACCEL_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;
    port->data_valid = false;
    result = osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
    return result;
}

static bool accel_port_set_calibration(
    accel_drv_t *dev, const accel_calibration_t *calibration)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;
    bsp_accel_calibration_t bsp_calibration;
    bool result;

    if (port == NULL || calibration == NULL || port->sample_mutex == NULL ||
        osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    bsp_calibration = (bsp_accel_calibration_t){
        .accel_offset_g = {
            calibration->accel_offset_g.x,
            calibration->accel_offset_g.y,
            calibration->accel_offset_g.z,
        },
        .accel_scale = {
            calibration->accel_scale.x,
            calibration->accel_scale.y,
            calibration->accel_scale.z,
        },
        .gyro_offset_dps = {
            calibration->gyro_offset_dps.x,
            calibration->gyro_offset_dps.y,
            calibration->gyro_offset_dps.z,
        },
    };
    result = accel_handler_set_calibration(
                 &port->handler, &bsp_calibration) == ACCEL_OK &&
             accel_port_invalidate_data(port);
    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool accel_port_get_calibration(
    accel_drv_t *dev, accel_calibration_t *calibration)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;
    bsp_accel_calibration_t bsp_calibration;
    bool result;

    if (port == NULL || calibration == NULL || port->sample_mutex == NULL ||
        osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    result = accel_handler_get_calibration(
                 &port->handler, &bsp_calibration) == ACCEL_OK;
    if (result) {
        calibration->accel_offset_g = (accel_vector3_t){
            bsp_calibration.accel_offset_g.x,
            bsp_calibration.accel_offset_g.y,
            bsp_calibration.accel_offset_g.z,
        };
        calibration->accel_scale = (accel_vector3_t){
            bsp_calibration.accel_scale.x,
            bsp_calibration.accel_scale.y,
            bsp_calibration.accel_scale.z,
        };
        calibration->gyro_offset_dps = (accel_vector3_t){
            bsp_calibration.gyro_offset_dps.x,
            bsp_calibration.gyro_offset_dps.y,
            bsp_calibration.gyro_offset_dps.z,
        };
    }

    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool accel_port_set_filter(accel_drv_t *dev,
                                  const accel_filter_config_t *config)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;
    bsp_accel_filter_config_t bsp_config;
    bool result;

    if (port == NULL || config == NULL || port->sample_mutex == NULL ||
        osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    bsp_config = (bsp_accel_filter_config_t){
        .sample_rate_hz = config->sample_rate_hz,
        .low_pass_cutoff_hz = config->low_pass_cutoff_hz,
        .median3_enabled = config->median3_enabled,
    };
    result = accel_handler_set_filter(&port->handler, &bsp_config) == ACCEL_OK &&
             accel_port_invalidate_data(port);
    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool accel_port_sleep(accel_drv_t *dev)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;
    bool result;

    if (port == NULL || port->sample_mutex == NULL ||
        osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    result = accel_handler_sleep(&port->handler) == ACCEL_OK;
    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool accel_port_wakeup(accel_drv_t *dev)
{
    accel_port_t *port = dev != NULL ? (accel_port_t *)dev->user_data : NULL;
    bool result;

    if (port == NULL || port->sample_mutex == NULL ||
        osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    result = accel_handler_wakeup(&port->handler) == ACCEL_OK &&
             accel_port_invalidate_data(port);
    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
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
    port->sample_mutex = NULL;
    port->data_mutex = NULL;
    port->snapshot = (accel_snapshot_t){0};
    port->data_valid = false;
    port->handler.initialized = false;

    driver = (accel_drv_t){
        .idx = index,
        .user_data = port,
        .init = accel_port_init,
        .refresh = accel_port_refresh,
        .read_cached = accel_port_read_cached,
        .read_snapshot = accel_port_read_snapshot,
        .set_calibration = accel_port_set_calibration,
        .get_calibration = accel_port_get_calibration,
        .set_filter = accel_port_set_filter,
        .sleep = accel_port_sleep,
        .wakeup = accel_port_wakeup,
    };

    if (!drv_adapter_accel_reg(index, &driver)) return false;

    port->registered = true;
    return true;
}
