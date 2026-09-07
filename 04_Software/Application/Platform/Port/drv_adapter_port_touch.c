#include "drv_adapter_port_touch.h"

#include "bsp_touch_handler.h"
#include "iic_hal.h"
#include "osal.h"

/**
 * @brief 单个触摸设备的 Port 私有装配和运行上下文。
 *
 * bus_mutex 只保护一整次 I2C 事务；operation_mutex 串行化 Handler 的
 * 滤波状态、配置修改和休眠/唤醒，获取顺序固定为 operation -> bus。
 */
typedef struct
{
    bool registered; /**< 是否已经占用对应 Wrapper 设备槽。 */
    touch_port_config_t config; /**< 注册时复制的板级控制配置。 */
    iic_bus_t iic_bus; /**< 注册时复制的软件 I2C 引脚描述。 */
    osal_mutex_handle_t bus_mutex; /**< 完整 I2C 事务互斥锁。 */
    osal_mutex_handle_t operation_mutex; /**< Handler 状态和设备操作互斥锁。 */
    touch_iic_interface_t iic_interface; /**< 注入 BSP 的 I2C 适配表。 */
    touch_control_interface_t control_interface; /**< 注入 BSP 的 GPIO 适配表。 */
    touch_yield_interface_t yield_interface; /**< 注入 BSP 的 OSAL 延时适配表。 */
    bsp_touch_handler_t handler; /**< 映射、滤波、状态机及具体驱动上下文。 */
} touch_port_t;

/** 每个 Wrapper 触摸索引对应一个独立的 Port 上下文。 */
static touch_port_t s_port[TOUCH_DEV_MAX];

#define TOUCH_OPERATION_TIMEOUT_MS (250U)

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
    bool initialized;

    if (port == NULL) return false;

    if (port->bus_mutex == NULL &&
        osal_mutex_create(&port->bus_mutex) != OSAL_SUCCESS)
        return false;

    if (port->operation_mutex == NULL &&
        osal_mutex_create(&port->operation_mutex) != OSAL_SUCCESS)
        return false;

    if (osal_mutex_take(port->operation_mutex,
                        TOUCH_OPERATION_TIMEOUT_MS) != OSAL_SUCCESS)
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

    initialized = touch_handler_init(&port->handler, &port->iic_interface,
                                     &port->control_interface,
                                     &port->yield_interface) == TOUCH_OK;
    if (osal_mutex_give(port->operation_mutex) != OSAL_SUCCESS) return false;
    return initialized;
}

static drv_adapter_touch_status_t touch_port_read(
    touch_drv_t *dev, drv_adapter_touch_point_t *point)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    touch_point_t raw_point;
    touch_status_t status;

    if (port == NULL || point == NULL || port->operation_mutex == NULL ||
        osal_mutex_take(port->operation_mutex,
                        TOUCH_OPERATION_TIMEOUT_MS) != OSAL_SUCCESS)
        return DRV_ADAPTER_TOUCH_ERROR;

    status = touch_handler_read(&port->handler, osal_time_get_ms(),
                                &raw_point);
    if (status == TOUCH_NO_TOUCH)
    {
        *point = (drv_adapter_touch_point_t){0};
        (void)osal_mutex_give(port->operation_mutex);
        return DRV_ADAPTER_TOUCH_NO_TOUCH;
    }
    if (status != TOUCH_OK)
    {
        (void)osal_mutex_give(port->operation_mutex);
        return DRV_ADAPTER_TOUCH_ERROR;
    }

    point->x = raw_point.x;
    point->y = raw_point.y;
    point->gesture = raw_point.gesture;
    point->event = raw_point.event;
    point->fingers = raw_point.fingers;
    point->timestamp_ms = raw_point.timestamp_ms;
    point->sequence = raw_point.sequence;

    return osal_mutex_give(port->operation_mutex) == OSAL_SUCCESS
         ? DRV_ADAPTER_TOUCH_OK : DRV_ADAPTER_TOUCH_ERROR;
}

static bool touch_port_sleep(touch_drv_t *dev)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    bool slept;

    if (port == NULL || port->operation_mutex == NULL ||
        osal_mutex_take(port->operation_mutex,
                        TOUCH_OPERATION_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    slept = touch_handler_sleep(&port->handler) == TOUCH_OK;
    return osal_mutex_give(port->operation_mutex) == OSAL_SUCCESS && slept;
}

static bool touch_port_get_info(touch_drv_t *dev,
                                drv_adapter_touch_info_t *info)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    touch_info_t bsp_info;
    bool succeeded;

    if (port == NULL || info == NULL || port->operation_mutex == NULL ||
        osal_mutex_take(port->operation_mutex,
                        TOUCH_OPERATION_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    succeeded = touch_handler_get_info(&port->handler, &bsp_info) == TOUCH_OK;
    if (!succeeded)
    {
        (void)osal_mutex_give(port->operation_mutex);
        return false;
    }

    *info = (drv_adapter_touch_info_t){
        .width = bsp_info.width,
        .height = bsp_info.height,
        .max_points = bsp_info.max_points,
    };
    return osal_mutex_give(port->operation_mutex) == OSAL_SUCCESS;
}

static void touch_config_to_bsp(
    const drv_adapter_touch_processing_config_t *source,
    touch_processing_config_t *destination)
{
    *destination = (touch_processing_config_t){
        .raw_x_min = source->raw_x_min,
        .raw_x_max = source->raw_x_max,
        .raw_y_min = source->raw_y_min,
        .raw_y_max = source->raw_y_max,
        .output_width = source->output_width,
        .output_height = source->output_height,
        .move_deadband_px = source->move_deadband_px,
        .fast_move_threshold_px_per_s =
            source->fast_move_threshold_px_per_s,
        .jump_threshold_px = source->jump_threshold_px,
        .jump_confirm_distance_px = source->jump_confirm_distance_px,
        .slow_filter_alpha_q8 = source->slow_filter_alpha_q8,
        .fast_filter_alpha_q8 = source->fast_filter_alpha_q8,
        .press_debounce_samples = source->press_debounce_samples,
        .release_debounce_samples = source->release_debounce_samples,
        .swap_xy = source->swap_xy,
        .invert_x = source->invert_x,
        .invert_y = source->invert_y,
        .median_filter_enabled = source->median_filter_enabled,
    };
}

static void touch_config_from_bsp(
    const touch_processing_config_t *source,
    drv_adapter_touch_processing_config_t *destination)
{
    *destination = (drv_adapter_touch_processing_config_t){
        .raw_x_min = source->raw_x_min,
        .raw_x_max = source->raw_x_max,
        .raw_y_min = source->raw_y_min,
        .raw_y_max = source->raw_y_max,
        .output_width = source->output_width,
        .output_height = source->output_height,
        .move_deadband_px = source->move_deadband_px,
        .fast_move_threshold_px_per_s =
            source->fast_move_threshold_px_per_s,
        .jump_threshold_px = source->jump_threshold_px,
        .jump_confirm_distance_px = source->jump_confirm_distance_px,
        .slow_filter_alpha_q8 = source->slow_filter_alpha_q8,
        .fast_filter_alpha_q8 = source->fast_filter_alpha_q8,
        .press_debounce_samples = source->press_debounce_samples,
        .release_debounce_samples = source->release_debounce_samples,
        .swap_xy = source->swap_xy,
        .invert_x = source->invert_x,
        .invert_y = source->invert_y,
        .median_filter_enabled = source->median_filter_enabled,
    };
}

static bool touch_port_set_processing_config(
    touch_drv_t *dev,
    const drv_adapter_touch_processing_config_t *config)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    touch_processing_config_t bsp_config;
    bool succeeded;

    if (port == NULL || config == NULL || port->operation_mutex == NULL ||
        osal_mutex_take(port->operation_mutex,
                        TOUCH_OPERATION_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    touch_config_to_bsp(config, &bsp_config);
    succeeded = touch_handler_set_processing_config(&port->handler,
                                                    &bsp_config) == TOUCH_OK;
    return osal_mutex_give(port->operation_mutex) == OSAL_SUCCESS &&
           succeeded;
}

static bool touch_port_get_processing_config(
    touch_drv_t *dev,
    drv_adapter_touch_processing_config_t *config)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    touch_processing_config_t bsp_config;
    bool succeeded;

    if (port == NULL || config == NULL || port->operation_mutex == NULL ||
        osal_mutex_take(port->operation_mutex,
                        TOUCH_OPERATION_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    succeeded = touch_handler_get_processing_config(&port->handler,
                                                    &bsp_config) == TOUCH_OK;
    if (succeeded) touch_config_from_bsp(&bsp_config, config);
    return osal_mutex_give(port->operation_mutex) == OSAL_SUCCESS &&
           succeeded;
}

static bool touch_port_wakeup(touch_drv_t *dev)
{
    touch_port_t *port = dev != NULL ? (touch_port_t *)dev->user_data : NULL;
    bool awakened;

    if (port == NULL || port->operation_mutex == NULL ||
        osal_mutex_take(port->operation_mutex,
                        TOUCH_OPERATION_TIMEOUT_MS) != OSAL_SUCCESS)
        return false;

    awakened = touch_handler_wakeup(&port->handler) == TOUCH_OK;
    return osal_mutex_give(port->operation_mutex) == OSAL_SUCCESS && awakened;
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
    port->operation_mutex = NULL;
    port->handler.initialized = false;

    driver = (touch_drv_t){
        .idx = index,
        .user_data = port,
        .init = touch_port_init,
        .read = touch_port_read,
        .get_info = touch_port_get_info,
        .set_processing_config = touch_port_set_processing_config,
        .get_processing_config = touch_port_get_processing_config,
        .sleep = touch_port_sleep,
        .wakeup = touch_port_wakeup,
    };

    if (!drv_adapter_touch_reg(index, &driver)) return false;

    port->registered = true;
    return true;
}
