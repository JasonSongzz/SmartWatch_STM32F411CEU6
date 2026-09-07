#include "drv_adapter_port_temphumi.h"

#include "bsp_temp_humi_handler.h"
#include "iic_hal.h"
#include "osal.h"

#define TEMPHUMI_MUTEX_TIMEOUT_MS (100U)
#define TEMPHUMI_DEFAULT_SAMPLE_INTERVAL_MS (100U)
#define TEMPHUMI_DEFAULT_SAMPLES_PER_GROUP  (10U)

_Static_assert(TEMP_HUMI_MIN_SAMPLES_PER_GROUP ==
               BSP_TEMP_HUMI_MIN_SAMPLES_PER_GROUP,
               "Wrapper and BSP minimum sample counts must match");

/**
 * @brief 单个温湿度设备的 Port 私有上下文。
 *
 * 锁职责：bus_mutex 只包围一次 I2C 事务；sample_mutex 串行化整组采样、
 * 校准和电源操作；data_mutex 只保护最近发布的温湿度缓存。
 * 需要嵌套时固定按 sample_mutex -> data_mutex 获取，禁止反向加锁。
 */
typedef struct
{
    bool registered; /**< 是否已经占用对应 Wrapper 设备槽。 */
    bool data_valid; /**< 缓存是否包含至少一组成功测量。 */
    float temperature; /**< 最近发布的温度，单位摄氏度。 */
    float humidity; /**< 最近发布的相对湿度，单位 %RH。 */
    temphumi_sample_config_t default_sampling; /**< refresh 使用的默认分组节奏。 */
    iic_bus_t iic_bus; /**< 注册时复制的软件 I2C 引脚描述。 */
    osal_mutex_handle_t bus_mutex; /**< 完整 I2C 事务互斥锁。 */
    osal_mutex_handle_t sample_mutex; /**< Handler 状态、整组采样和设备操作锁。 */
    osal_mutex_handle_t data_mutex; /**< 温湿度缓存及 data_valid 发布锁。 */
    temp_humi_iic_interface_t iic_interface; /**< 注入 BSP 的 I2C 适配表。 */
    temp_humi_yield_interface_t yield_interface; /**< 注入 BSP 的 OSAL 延时适配。 */
    bsp_temp_humi_handler_t handler; /**< 校准、分组滤波及具体驱动上下文。 */
} temphumi_port_t;

/** 每个已注册设备拥有独立上下文，索引与 Wrapper 设备索引一致。 */
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

    if (port->sample_mutex == NULL &&
        osal_mutex_create(&port->sample_mutex) != OSAL_SUCCESS)
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

    port->data_valid = false;
    return temp_humi_handler_init(&port->handler, &port->iic_interface,
                                  &port->yield_interface) == TEMP_HUMI_OK;
}

static bool temphumi_port_sample_group(
    temphumi_drv_t *dev, const temphumi_sample_config_t *config)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;
    float temperature;
    float humidity;
    bool result;

    if (port == NULL || config == NULL ||
        config->samples_per_group < TEMP_HUMI_MIN_SAMPLES_PER_GROUP ||
        port->sample_mutex == NULL)
        return false;

    if (osal_mutex_take(port->sample_mutex,
                        TEMPHUMI_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    if (temp_humi_handler_sample_group(
            &port->handler, config->sample_interval_ms,
            config->samples_per_group, &temperature,
            &humidity) != TEMP_HUMI_OK) {
        (void)osal_mutex_give(port->sample_mutex);
        return false;
    }

    if (osal_mutex_take(port->data_mutex,
                        TEMPHUMI_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS) {
        (void)osal_mutex_give(port->sample_mutex);
        return false;
    }

    port->temperature = temperature;
    port->humidity = humidity;
    port->data_valid = true;

    result = osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool temphumi_port_refresh(temphumi_drv_t *dev)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;

    return port != NULL &&
           temphumi_port_sample_group(dev, &port->default_sampling);
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

    if (!port->data_valid) {
        (void)osal_mutex_give(port->data_mutex);
        return false;
    }

    *temperature = port->temperature;
    *humidity = port->humidity;

    return osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
}

static bool temphumi_port_set_calibration(
    temphumi_drv_t *dev, const temphumi_calibration_t *calibration)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;
    bsp_temp_humi_calibration_t bsp_calibration;
    bool result;

    if (port == NULL || calibration == NULL || port->sample_mutex == NULL)
        return false;

    bsp_calibration = (bsp_temp_humi_calibration_t){
        .temperature_scale = calibration->temperature_scale,
        .temperature_offset = calibration->temperature_offset,
        .humidity_scale = calibration->humidity_scale,
        .humidity_offset = calibration->humidity_offset,
    };

    if (osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    result = temp_humi_handler_set_calibration(
                 &port->handler, &bsp_calibration) == TEMP_HUMI_OK;
    if (result) {
        if (port->data_mutex == NULL ||
            osal_mutex_take(port->data_mutex,
                            TEMPHUMI_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS) {
            result = false;
        } else {
            port->data_valid = false;
            result = osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
        }
    }

    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool temphumi_port_get_calibration(
    temphumi_drv_t *dev, temphumi_calibration_t *calibration)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;
    bsp_temp_humi_calibration_t bsp_calibration;
    bool result;

    if (port == NULL || calibration == NULL || port->sample_mutex == NULL)
        return false;

    if (osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    result = temp_humi_handler_get_calibration(
                 &port->handler, &bsp_calibration) == TEMP_HUMI_OK;
    if (result) {
        calibration->temperature_scale = bsp_calibration.temperature_scale;
        calibration->temperature_offset = bsp_calibration.temperature_offset;
        calibration->humidity_scale = bsp_calibration.humidity_scale;
        calibration->humidity_offset = bsp_calibration.humidity_offset;
    }

    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool temphumi_port_sleep(temphumi_drv_t *dev)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;
    bool result;

    if (port == NULL || port->sample_mutex == NULL ||
        osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    result = temp_humi_handler_sleep(&port->handler) == TEMP_HUMI_OK;
    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
}

static bool temphumi_port_wakeup(temphumi_drv_t *dev)
{
    temphumi_port_t *port = dev != NULL
                          ? (temphumi_port_t *)dev->user_data : NULL;
    bool result;

    if (port == NULL || port->sample_mutex == NULL ||
        osal_mutex_take(port->sample_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    result = temp_humi_handler_wakeup(&port->handler) == TEMP_HUMI_OK;
    if (result) {
        if (osal_mutex_take(port->data_mutex,
                            TEMPHUMI_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS) {
            result = false;
        } else {
            port->data_valid = false;
            result = osal_mutex_give(port->data_mutex) == OSAL_SUCCESS;
        }
    }

    return osal_mutex_give(port->sample_mutex) == OSAL_SUCCESS && result;
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

    if (config != NULL && config->samples_per_group != 0U &&
        config->samples_per_group < TEMP_HUMI_MIN_SAMPLES_PER_GROUP)
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
    port->default_sampling.sample_interval_ms =
        config != NULL && config->sample_interval_ms > 0U
        ? config->sample_interval_ms : TEMPHUMI_DEFAULT_SAMPLE_INTERVAL_MS;
    port->default_sampling.samples_per_group =
        config != NULL && config->samples_per_group > 0U
        ? config->samples_per_group : TEMPHUMI_DEFAULT_SAMPLES_PER_GROUP;
    port->temperature = 0.0f;
    port->humidity = 0.0f;
    port->data_valid = false;
    port->handler.initialized = false;

    driver = (temphumi_drv_t){
        .idx = index,
        .user_data = port,
        .init = temphumi_port_init,
        .refresh = temphumi_port_refresh,
        .sample_group = temphumi_port_sample_group,
        .read_cached = temphumi_port_read_cached,
        .set_calibration = temphumi_port_set_calibration,
        .get_calibration = temphumi_port_get_calibration,
        .sleep = temphumi_port_sleep,
        .wakeup = temphumi_port_wakeup,
    };

    if (!drv_adapter_temphumi_reg(index, &driver)) return false;

    port->registered = true;
    return true;
}
