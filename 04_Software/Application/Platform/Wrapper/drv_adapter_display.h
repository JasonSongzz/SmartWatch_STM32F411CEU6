#ifndef DRV_ADAPTER_DISPLAY_H
#define DRV_ADAPTER_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DISPLAY_DEV_MAX (1U)

/** @brief 显示设备向 Service/UI 暴露的逻辑能力。 */
typedef struct
{
    uint16_t width; /**< 逻辑可见宽度，单位像素。 */
    uint16_t height; /**< 逻辑可见高度，单位像素。 */
    uint16_t x_offset; /**< 控制器显存列偏移，单位像素。 */
    uint16_t y_offset; /**< 控制器显存行偏移，单位像素。 */
    uint8_t bits_per_pixel; /**< 单像素位宽。 */
    bool requires_byte_swap; /**< RGB565 数据是否需要交换高低字节。 */
} drv_adapter_display_info_t;

/**
 * @brief Wrapper 保存的显示设备操作表。
 * @note Port 注册此表，Service/UI 仅通过 drv_adapter_display_* 访问设备。
 */
typedef struct display_drv
{
    uint32_t idx; /**< Wrapper 写入的设备索引。 */
    void *user_data; /**< Port 私有上下文，Wrapper 不解析。 */
    bool (*init)(struct display_drv *dev); /**< 初始化 Port、Handler 和面板。 */
    bool (*write_area)(struct display_drv *dev, uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1, const uint8_t *pixels,
                       size_t size); /**< 原子设置窗口并写入完整像素流。 */
    bool (*fill)(struct display_drv *dev, uint16_t color); /**< 全屏填充 RGB565。 */
    bool (*get_info)(struct display_drv *dev,
                     drv_adapter_display_info_t *info); /**< 获取逻辑画布能力。 */
    bool (*sleep)(struct display_drv *dev); /**< 进入低功耗显示状态。 */
    bool (*wakeup)(struct display_drv *dev); /**< 唤醒并恢复显示。 */
} display_drv_t;

/** @brief 注册显示设备操作表；只复制结构，不访问硬件。 */
bool drv_adapter_display_reg(uint32_t index, const display_drv_t *dev);
/** @brief 初始化指定显示设备。 */
bool drv_adapter_display_init(uint32_t index);
/** @brief 在一个不可分割的设备事务内设置窗口并写入完整像素流。 */
bool drv_adapter_display_write_area(uint32_t index,
                                    uint16_t x0, uint16_t y0,
                                    uint16_t x1, uint16_t y1,
                                    const uint8_t *pixels, size_t size);
/** @brief 使用 RGB565 颜色填充整个屏幕。 */
bool drv_adapter_display_fill(uint32_t index, uint16_t color);
/** @brief 获取显示分辨率、偏移和像素格式信息。 */
bool drv_adapter_display_get_info(uint32_t index,
                                  drv_adapter_display_info_t *info);
/** @brief 使指定显示设备进入休眠。 */
bool drv_adapter_display_sleep(uint32_t index);
/** @brief 唤醒指定显示设备。 */
bool drv_adapter_display_wakeup(uint32_t index);

#endif /* DRV_ADAPTER_DISPLAY_H */
