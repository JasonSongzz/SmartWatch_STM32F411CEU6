#ifndef BSP_DISPLAY_HANDLER_H
#define BSP_DISPLAY_HANDLER_H

/* Change only this include when replacing the display controller. */
#include "bsp_st7789t3_driver.h"

#include <stdbool.h>

/** @brief 显示 Handler 上下文，隔离具体显示控制器型号。 */
typedef struct
{
    bsp_display_driver_t driver; /**< 当前装配的具体显示驱动。 */
    bool initialized; /**< 底层驱动是否已成功初始化。 */
} bsp_display_handler_t;

/** @brief 装配显示驱动并初始化面板。 */
display_status_t display_handler_init(
    bsp_display_handler_t *handler, const display_spi_interface_t *spi,
    const display_control_interface_t *control,
    const display_delay_interface_t *delay);
/** @brief 原子设置逻辑矩形窗口并写入完整像素字节流，坐标包含端点。 */
display_status_t display_handler_write_area(
    bsp_display_handler_t *handler, uint16_t x0, uint16_t y0,
    uint16_t x1, uint16_t y1, const uint8_t *pixels, size_t size);
/** @brief 使用一个 RGB565 颜色填充整个逻辑画布。 */
display_status_t display_handler_fill(bsp_display_handler_t *handler,
                                      uint16_t color);
/** @brief 获取与具体显示控制器无关的画布能力。 */
display_status_t display_handler_get_info(
    const bsp_display_handler_t *handler, display_info_t *info);
/** @brief 让显示控制器进入低功耗状态。 */
display_status_t display_handler_sleep(bsp_display_handler_t *handler);
/** @brief 唤醒显示控制器并恢复显示能力。 */
display_status_t display_handler_wakeup(bsp_display_handler_t *handler);

#endif /* BSP_DISPLAY_HANDLER_H */
