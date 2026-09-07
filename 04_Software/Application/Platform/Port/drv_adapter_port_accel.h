#ifndef DRV_ADAPTER_PORT_ACCEL_H
#define DRV_ADAPTER_PORT_ACCEL_H

#include "drv_adapter_accel.h"
#include "osal_mutex.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* Temporary software-I2C pins. Change only these four macros for new wiring. */
#ifndef ACCEL_IIC_SCL_PORT
#define ACCEL_IIC_SCL_PORT GPIOB
#endif
#ifndef ACCEL_IIC_SCL_PIN
#define ACCEL_IIC_SCL_PIN GPIO_PIN_8
#endif
#ifndef ACCEL_IIC_SDA_PORT
#define ACCEL_IIC_SDA_PORT GPIOB
#endif
#ifndef ACCEL_IIC_SDA_PIN
#define ACCEL_IIC_SDA_PIN GPIO_PIN_9
#endif

/**
 * @brief 加速度计 Port 的板级装配参数。
 *
 * iic_bus 实际指向 iic_bus_t，但在公开头文件中保持 void *，避免 Wrapper
 * 和 Service 感知软件 I2C 实现。结构内容会在注册时复制。
 */
typedef struct
{
    void *iic_bus; /**< 可选的 iic_bus_t；为空时使用本文件默认引脚。 */
    osal_mutex_handle_t bus_mutex; /**< 可选共享总线锁；为空时由 Port 创建。 */
} accel_port_config_t;

/**
 * @brief 把一个板级加速度计实例注册到 Wrapper。
 * @param index Wrapper 设备索引，必须小于 ACCEL_DEV_MAX。
 * @param config 板级配置；传 NULL 使用默认软件 I2C 引脚和新建互斥锁。
 * @return true 表示注册成功；本函数不访问传感器硬件。
 */
bool drv_adapter_port_accel_register(uint32_t index,
                                     const accel_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_ACCEL_H */
