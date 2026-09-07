#ifndef DRV_ADAPTER_PORT_FLASH_H
#define DRV_ADAPTER_PORT_FLASH_H

#include "drv_adapter_flash.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 注册板载 SPI Flash、FAL 分区和 FlashDB 操作表。
 * @param index Wrapper 设备索引，通常使用 FLASH_DEV_EXTERNAL。
 * @return true 表示注册成功；物理探测和数据库初始化延迟到 Wrapper init。
 */
bool drv_adapter_port_flash_register(uint32_t index);

#endif /* DRV_ADAPTER_PORT_FLASH_H */
