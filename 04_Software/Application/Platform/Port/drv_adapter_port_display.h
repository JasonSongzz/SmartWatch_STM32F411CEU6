#ifndef DRV_ADAPTER_PORT_DISPLAY_H
#define DRV_ADAPTER_PORT_DISPLAY_H

#include "drv_adapter_display.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* Default board wiring. Override these macros when the MCU pin map changes. */
#ifndef DISPLAY_CS_PORT
#define DISPLAY_CS_PORT GPIOA
#endif
#ifndef DISPLAY_CS_PIN
#define DISPLAY_CS_PIN GPIO_PIN_4
#endif
#ifndef DISPLAY_DC_PORT
#define DISPLAY_DC_PORT GPIOB
#endif
#ifndef DISPLAY_DC_PIN
#define DISPLAY_DC_PIN GPIO_PIN_0
#endif
#ifndef DISPLAY_RESET_PORT
#define DISPLAY_RESET_PORT GPIOB
#endif
#ifndef DISPLAY_RESET_PIN
#define DISPLAY_RESET_PIN GPIO_PIN_1
#endif
#ifndef DISPLAY_BL_PORT
#define DISPLAY_BL_PORT GPIOB
#endif
#ifndef DISPLAY_BL_PIN
#define DISPLAY_BL_PIN GPIO_PIN_2
#endif

/** @brief 显示 Port 的 MCU 外设和控制引脚装配参数。 */
typedef struct
{
    SPI_HandleTypeDef *spi; /**< CubeMX 生成的 SPI 句柄。 */
    GPIO_TypeDef *cs_port; /**< 片选 GPIO 端口。 */
    uint16_t cs_pin; /**< 片选 GPIO 引脚掩码。 */
    GPIO_TypeDef *dc_port; /**< 数据/命令 GPIO 端口。 */
    uint16_t dc_pin; /**< 数据/命令 GPIO 引脚掩码。 */
    GPIO_TypeDef *reset_port; /**< 硬件复位 GPIO 端口。 */
    uint16_t reset_pin; /**< 硬件复位 GPIO 引脚掩码。 */
    GPIO_TypeDef *bl_port; /**< 可选背光 GPIO 端口；NULL 表示未连接。 */
    uint16_t bl_pin; /**< 可选背光 GPIO 引脚掩码。 */
} display_port_config_t;

/**
 * @brief 将板级显示设备装配到 Display Wrapper。
 * @param index Wrapper 设备索引。
 * @param config 板级配置；NULL 使用默认 SPI 句柄和上述引脚宏。
 * @return true 表示注册成功；硬件初始化延迟到 Wrapper init 调用。
 */
bool drv_adapter_port_display_register(
    uint32_t index, const display_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_DISPLAY_H */
