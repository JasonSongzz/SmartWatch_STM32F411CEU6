#ifndef DRV_ADAPTER_PORT_TOUCH_H
#define DRV_ADAPTER_PORT_TOUCH_H

#include "drv_adapter_touch.h"
#include "osal_mutex.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* Temporary software-I2C/control pins. Change these macros for new wiring. */
#ifndef TOUCH_IIC_SCL_PORT
#define TOUCH_IIC_SCL_PORT GPIOA
#endif
#ifndef TOUCH_IIC_SCL_PIN
#define TOUCH_IIC_SCL_PIN GPIO_PIN_8
#endif
#ifndef TOUCH_IIC_SDA_PORT
#define TOUCH_IIC_SDA_PORT GPIOA
#endif
#ifndef TOUCH_IIC_SDA_PIN
#define TOUCH_IIC_SDA_PIN GPIO_PIN_9
#endif
#ifndef TOUCH_RESET_PORT
#define TOUCH_RESET_PORT GPIOA
#endif
#ifndef TOUCH_RESET_PIN
#define TOUCH_RESET_PIN GPIO_PIN_10
#endif
#ifndef TOUCH_INTERRUPT_PORT
#define TOUCH_INTERRUPT_PORT GPIOA
#endif
#ifndef TOUCH_INTERRUPT_PIN
#define TOUCH_INTERRUPT_PIN GPIO_PIN_11
#endif

/** @brief 触摸 Port 的软件 I2C、复位和中断装配参数。 */
typedef struct
{
    void *iic_bus; /**< 可选的 iic_bus_t；NULL 时使用默认软件 I2C 引脚。 */
    osal_mutex_handle_t bus_mutex; /**< 可选共享总线锁；NULL 时由 Port 创建。 */
    GPIO_TypeDef *reset_port; /**< 触摸控制器复位 GPIO 端口。 */
    uint16_t reset_pin; /**< 复位 GPIO 引脚掩码。 */
    GPIO_TypeDef *interrupt_port; /**< 可选中断 GPIO 端口；NULL 表示不查询。 */
    uint16_t interrupt_pin; /**< 可选中断 GPIO 引脚掩码。 */
    bool interrupt_active_low; /**< true 表示低电平为中断有效。 */
} touch_port_config_t;

/**
 * @brief 将板级触摸设备装配到 Touch Wrapper。
 * @param index Wrapper 设备索引。
 * @param config 板级配置；NULL 使用默认引脚并创建互斥锁。
 * @return true 表示注册成功；硬件初始化延迟到 Wrapper init 调用。
 */
bool drv_adapter_port_touch_register(uint32_t index,
                                     const touch_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_TOUCH_H */
