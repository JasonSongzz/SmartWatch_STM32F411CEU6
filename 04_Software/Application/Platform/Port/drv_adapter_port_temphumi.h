#ifndef DRV_ADAPTER_PORT_TEMPHUMI_H
#define DRV_ADAPTER_PORT_TEMPHUMI_H

#include "drv_adapter_temphumi.h"
#include "osal_mutex.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* Temporary software-I2C pins. Change only these four macros for new wiring. */
#ifndef TEMP_HUMI_IIC_SCL_PORT
#define TEMP_HUMI_IIC_SCL_PORT GPIOB
#endif
#ifndef TEMP_HUMI_IIC_SCL_PIN
#define TEMP_HUMI_IIC_SCL_PIN GPIO_PIN_6
#endif
#ifndef TEMP_HUMI_IIC_SDA_PORT
#define TEMP_HUMI_IIC_SDA_PORT GPIOB
#endif
#ifndef TEMP_HUMI_IIC_SDA_PIN
#define TEMP_HUMI_IIC_SDA_PIN GPIO_PIN_7
#endif

/**
 * @brief 温湿度 Port 的板级装配和默认采样参数。
 *
 * iic_bus 实际指向 iic_bus_t，结构内容在注册时复制；sample_interval_ms
 * 和 samples_per_group 只供 refresh 默认采样使用，Service 仍可逐次覆盖。
 */
typedef struct
{
    void *iic_bus; /**< 可选的 iic_bus_t；为空时使用本文件默认引脚。 */
    osal_mutex_handle_t bus_mutex; /**< 可选共享总线锁；为空时由 Port 创建。 */
    uint32_t sample_interval_ms; /**< 默认相邻采样间隔，单位 ms；0 使用 Port 默认值。 */
    uint16_t samples_per_group; /**< 默认分组点数；0 使用 Port 默认值。 */
} temphumi_port_config_t;

/**
 * @brief 把一个板级温湿度实例注册到 Wrapper。
 * @param index Wrapper 设备索引，必须小于 TEMP_HUMI_DEV_MAX。
 * @param config 板级配置；传 NULL 使用默认软件 I2C 引脚和采样参数。
 * @return true 表示注册成功；本函数不访问传感器硬件。
 */
bool drv_adapter_port_temphumi_register(
    uint32_t index, const temphumi_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_TEMPHUMI_H */
