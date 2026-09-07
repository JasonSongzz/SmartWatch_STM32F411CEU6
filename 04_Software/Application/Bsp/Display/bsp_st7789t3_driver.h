#ifndef BSP_ST7789T3_DRIVER_H
#define BSP_ST7789T3_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ST7789T3 protocol configuration. */
#define ST7789T3_CMD_SWRESET           0x01U
#define ST7789T3_CMD_SLPIN             0x10U
#define ST7789T3_CMD_SLPOUT            0x11U
#define ST7789T3_CMD_MADCTL            0x36U
#define ST7789T3_CMD_COLMOD            0x3AU
#define ST7789T3_CMD_CASET             0x2AU
#define ST7789T3_CMD_RASET             0x2BU
#define ST7789T3_CMD_RAMWR             0x2CU
#define ST7789T3_CMD_DISPON            0x29U
#define ST7789T3_CMD_DISPOFF           0x28U
#define ST7789T3_PIXEL_FORMAT_RGB565   0x55U
#define ST7789T3_WIDTH                 240U
#define ST7789T3_HEIGHT                280U
#define ST7789T3_X_OFFSET              0U
#define ST7789T3_Y_OFFSET              0U
#define ST7789T3_MADCTL                0x00U
#define ST7789T3_BITS_PER_PIXEL        16U
#define ST7789T3_REQUIRES_BYTE_SWAP    true

/** @brief 显示 BSP 统一返回状态。 */
typedef enum
{
    DISPLAY_OK = 0,          /**< 操作成功。 */
    DISPLAY_ERROR,           /**< SPI 或显示控制器操作失败。 */
    DISPLAY_ERROR_PARAMETER, /**< 空指针、空数据或坐标范围非法。 */
    DISPLAY_ERROR_RESOURCE,  /**< 驱动未初始化或所需资源不可用。 */
    DISPLAY_ERROR_TIMEOUT    /**< SPI 事务或互斥锁等待超时。 */
} display_status_t;

/**
 * @brief 显示驱动使用的 SPI 抽象接口。
 *
 * BSP 不感知 HAL 和 OSAL。同步写用于命令，DMA 写用于像素流；
 * pf_lock/pf_unlock 保护包含命令、地址窗口和数据在内的完整事务。
 */
typedef struct
{
    void *bus_context; /**< 由 Port 持有并原样回传的 SPI 上下文。 */
    display_status_t (*pf_spi_write)(void *bus_context,
                                     const uint8_t *data, size_t size,
                                     uint32_t timeout_ms); /**< 同步发送字节流。 */
    display_status_t (*pf_spi_write_dma)(void *bus_context,
                                         const uint8_t *data, size_t size); /**< 启动异步 DMA 发送。 */
    display_status_t (*pf_spi_wait_complete)(void *bus_context,
                                             uint32_t timeout_ms); /**< 等待 DMA 完成。 */
    display_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms); /**< 锁定完整显示事务。 */
    display_status_t (*pf_unlock)(void *bus_context); /**< 释放显示事务锁。 */
} display_spi_interface_t;

/** @brief 显示控制器所需 GPIO 控制接口。 */
typedef struct
{
    void *context; /**< 由 Port 持有并回传的 GPIO 上下文。 */
    void (*pf_set_cs)(void *context, bool high); /**< 设置片选引脚电平。 */
    void (*pf_set_dc)(void *context, bool data_mode); /**< true 选择数据，false 选择命令。 */
    void (*pf_set_reset)(void *context, bool high); /**< 设置硬件复位引脚电平。 */
    void (*pf_set_backlight)(void *context, bool on); /**< 控制背光；允许 Port 忽略。 */
} display_control_interface_t;

/** @brief 显示初始化时使用的阻塞延时抽象。 */
typedef struct
{
    void *context; /**< 由 Port 持有并回传的延时上下文。 */
    void (*pf_delay_ms)(void *context, uint32_t milliseconds); /**< OSAL 毫秒延时。 */
} display_delay_interface_t;

/** @brief 显示控制器向上层报告的逻辑画布信息。 */
typedef struct
{
    uint16_t width; /**< 逻辑可见宽度，单位像素。 */
    uint16_t height; /**< 逻辑可见高度，单位像素。 */
    uint16_t x_offset; /**< 逻辑 X=0 对应显存列偏移。 */
    uint16_t y_offset; /**< 逻辑 Y=0 对应显存行偏移。 */
    uint8_t bits_per_pixel; /**< 像素位宽。 */
    bool requires_byte_swap; /**< RGB565 字节序是否需由上层交换。 */
} display_info_t;

/** @brief 允许驱动操作表回调引用自身实例的前向类型。 */
typedef struct bsp_display_driver bsp_display_driver_t;

/** @brief BSP 显示控制器实例及型号无关操作表。 */
struct bsp_display_driver
{
    const display_spi_interface_t *spi; /**< Port 注入的 SPI 接口。 */
    const display_control_interface_t *control; /**< Port 注入的 GPIO 接口。 */
    const display_delay_interface_t *delay; /**< Port 注入的延时接口。 */
    uint16_t width; /**< 当前逻辑宽度，像素。 */
    uint16_t height; /**< 当前逻辑高度，像素。 */
    uint16_t x_offset; /**< 显存列偏移，像素。 */
    uint16_t y_offset; /**< 显存行偏移，像素。 */
    uint8_t madctl; /**< 当前扫描方向/颜色顺序寄存器值。 */
    bool initialized; /**< 控制器初始化成功标志。 */

    display_status_t (*pf_init)(bsp_display_driver_t *display); /**< 初始化控制器。 */
    display_status_t (*pf_write_area)(bsp_display_driver_t *display,
                                     uint16_t x0, uint16_t y0,
                                     uint16_t x1, uint16_t y1,
                                     const uint8_t *pixels,
                                     size_t size); /**< 在同一次总线锁内设置窗口并写入完整像素流。 */
    display_status_t (*pf_fill)(bsp_display_driver_t *display, uint16_t color); /**< 全屏填充 RGB565。 */
    display_status_t (*pf_get_info)(const bsp_display_driver_t *display,
                                    display_info_t *info); /**< 获取画布能力。 */
    display_status_t (*pf_sleep)(bsp_display_driver_t *display); /**< 关闭显示并休眠。 */
    display_status_t (*pf_wakeup)(bsp_display_driver_t *display); /**< 唤醒并恢复显示。 */
};

/** @brief 装配并初始化 ST7789T3 通用显示实例。 */
display_status_t st7789t3_inst(bsp_display_driver_t *display,
                               const display_spi_interface_t *spi,
                               const display_control_interface_t *control,
                               const display_delay_interface_t *delay);

#define bsp_display_inst st7789t3_inst

#endif /* BSP_ST7789T3_DRIVER_H */
