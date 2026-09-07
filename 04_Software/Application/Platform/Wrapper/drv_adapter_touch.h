#ifndef DRV_ADAPTER_TOUCH_H
#define DRV_ADAPTER_TOUCH_H

#include <stdbool.h>
#include <stdint.h>

#define TOUCH_DEV_MAX (2U)

/** @brief Touch Wrapper 的读取结果。 */
typedef enum
{
    DRV_ADAPTER_TOUCH_OK = 0, /**< 返回了有效 DOWN/MOVE/UP 事件。 */
    DRV_ADAPTER_TOUCH_NO_TOUCH, /**< 当前无触点且没有待发布释放事件。 */
    DRV_ADAPTER_TOUCH_ERROR /**< 参数、设备或总线操作失败。 */
} drv_adapter_touch_status_t;

/** @brief Service/UI 可直接消费的通用触点事件。 */
typedef enum
{
    DRV_ADAPTER_TOUCH_EVENT_NONE = 0, /**< 无事件。 */
    DRV_ADAPTER_TOUCH_EVENT_DOWN, /**< 新触点按下。 */
    DRV_ADAPTER_TOUCH_EVENT_MOVE, /**< 触点保持或移动。 */
    DRV_ADAPTER_TOUCH_EVENT_UP /**< 触点释放。 */
} drv_adapter_touch_event_t;

/** @brief Wrapper 输出的单触点事件及元数据。 */
typedef struct
{
    uint16_t x; /**< 已映射和滤波的逻辑 X 坐标。 */
    uint16_t y; /**< 已映射和滤波的逻辑 Y 坐标。 */
    uint8_t gesture; /**< 芯片报告的手势标识。 */
    uint8_t event; /**< drv_adapter_touch_event_t。 */
    uint8_t fingers; /**< 当前有效触点数。 */
    uint32_t timestamp_ms; /**< 采样时的单调时钟毫秒值。 */
    uint32_t sequence; /**< 每次发布事件递增的序号。 */
} drv_adapter_touch_point_t;

/** @brief 触摸设备的逻辑坐标能力。 */
typedef struct
{
    uint16_t width; /**< 输出坐标宽度。 */
    uint16_t height; /**< 输出坐标高度。 */
    uint8_t max_points; /**< 支持的最大同时触点数。 */
} drv_adapter_touch_info_t;

/** @brief 与触摸控制器型号无关的坐标映射和滤波配置。 */
typedef struct
{
    uint16_t raw_x_min; /**< 有效原始 X 最小值。 */
    uint16_t raw_x_max; /**< 有效原始 X 最大值。 */
    uint16_t raw_y_min; /**< 有效原始 Y 最小值。 */
    uint16_t raw_y_max; /**< 有效原始 Y 最大值。 */
    uint16_t output_width; /**< 目标逻辑宽度，像素。 */
    uint16_t output_height; /**< 目标逻辑高度，像素。 */
    uint16_t move_deadband_px; /**< 静止抖动死区，像素。 */
    uint16_t fast_move_threshold_px_per_s; /**< 快速滤波切换阈值。 */
    uint16_t jump_threshold_px; /**< 大跳点确认阈值；0 禁用。 */
    uint16_t jump_confirm_distance_px; /**< 大跳点二次确认容差。 */
    uint8_t slow_filter_alpha_q8; /**< 慢速 IIR 系数，1~255。 */
    uint8_t fast_filter_alpha_q8; /**< 快速 IIR 系数，1~255。 */
    uint8_t press_debounce_samples; /**< 确认按下所需连续点数。 */
    uint8_t release_debounce_samples; /**< 确认释放所需连续丢点数。 */
    bool swap_xy; /**< 是否交换 X/Y 轴来源。 */
    bool invert_x; /**< 是否反转原始 X 方向。 */
    bool invert_y; /**< 是否反转原始 Y 方向。 */
    bool median_filter_enabled; /**< 是否启用三点中值滤波。 */
} drv_adapter_touch_processing_config_t;

/** @brief Wrapper 保存的触摸设备操作表。 */
typedef struct touch_drv
{
    uint32_t idx; /**< Wrapper 写入的设备索引。 */
    void *user_data; /**< Port 私有上下文，Wrapper 不解析。 */
    bool (*init)(struct touch_drv *dev); /**< 初始化 Port、Handler 和控制器。 */
    drv_adapter_touch_status_t (*read)(
        struct touch_drv *dev, drv_adapter_touch_point_t *point); /**< 读取一个通用触摸事件。 */
    bool (*get_info)(struct touch_drv *dev,
                     drv_adapter_touch_info_t *info); /**< 获取逻辑坐标能力。 */
    bool (*set_processing_config)(
        struct touch_drv *dev,
        const drv_adapter_touch_processing_config_t *config); /**< 设置映射和滤波配置。 */
    bool (*get_processing_config)(
        struct touch_drv *dev,
        drv_adapter_touch_processing_config_t *config); /**< 获取当前处理配置。 */
    bool (*sleep)(struct touch_drv *dev); /**< 进入低功耗状态。 */
    bool (*wakeup)(struct touch_drv *dev); /**< 唤醒并重置触摸状态机。 */
} touch_drv_t;

/** @brief 注册触摸设备操作表；只复制结构，不访问硬件。 */
bool drv_adapter_touch_reg(uint32_t index, const touch_drv_t *dev);
/** @brief 初始化指定触摸设备。 */
bool drv_adapter_touch_init(uint32_t index);
/** @brief 读取一个经过映射、消抖和滤波的触摸事件。 */
drv_adapter_touch_status_t drv_adapter_touch_read(
    uint32_t index, drv_adapter_touch_point_t *point);
/** @brief 获取当前逻辑坐标范围和最大触点数。 */
bool drv_adapter_touch_get_info(uint32_t index,
                                drv_adapter_touch_info_t *info);
/** @brief 更新坐标映射及滤波配置；成功后重置当前触摸状态。 */
bool drv_adapter_touch_set_processing_config(
    uint32_t index,
    const drv_adapter_touch_processing_config_t *config);
/** @brief 获取当前坐标映射及滤波配置。 */
bool drv_adapter_touch_get_processing_config(
    uint32_t index,
    drv_adapter_touch_processing_config_t *config);
/** @brief 使指定触摸设备进入休眠。 */
bool drv_adapter_touch_sleep(uint32_t index);
/** @brief 唤醒指定触摸设备。 */
bool drv_adapter_touch_wakeup(uint32_t index);

#endif /* DRV_ADAPTER_TOUCH_H */
