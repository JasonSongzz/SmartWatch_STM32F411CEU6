#ifndef BSP_TOUCH_HANDLER_H
#define BSP_TOUCH_HANDLER_H

/* Change only this include when replacing the touch controller. */
#include "bsp_cst816t_driver.h"

#include <stdbool.h>

/**
 * @brief 触摸坐标标定、状态消抖和滤波配置。
 *
 * 原始坐标先按 min/max 归一化，再执行翻转和 XY 交换，最后映射到
 * output_width/output_height；越界点被拒绝而不是钳位到屏幕边缘。
 */
typedef struct
{
    uint16_t raw_x_min; /**< 有效原始 X 最小值，包含端点。 */
    uint16_t raw_x_max; /**< 有效原始 X 最大值，包含端点。 */
    uint16_t raw_y_min; /**< 有效原始 Y 最小值，包含端点。 */
    uint16_t raw_y_max; /**< 有效原始 Y 最大值，包含端点。 */
    uint16_t output_width; /**< 映射后的逻辑宽度，像素。 */
    uint16_t output_height; /**< 映射后的逻辑高度，像素。 */
    uint16_t move_deadband_px; /**< 小于该距离的静止抖动保持上一坐标。 */
    uint16_t fast_move_threshold_px_per_s; /**< 切换到快速 IIR 系数的速度阈值。 */
    uint16_t jump_threshold_px; /**< 需二次确认的大跳点曼哈顿距离；0 禁用。 */
    uint16_t jump_confirm_distance_px; /**< 两个大跳点可视为同一目标的距离。 */
    uint8_t slow_filter_alpha_q8; /**< 慢速移动 IIR 系数，范围 1~255。 */
    uint8_t fast_filter_alpha_q8; /**< 快速移动 IIR 系数，范围 1~255。 */
    uint8_t press_debounce_samples; /**< 生成 DOWN 前所需连续有效点数。 */
    uint8_t release_debounce_samples; /**< 生成 UP 前所需连续丢点数。 */
    bool swap_xy; /**< 是否交换映射后的 X/Y 轴来源。 */
    bool invert_x; /**< 是否反转原始 X 方向。 */
    bool invert_y; /**< 是否反转原始 Y 方向。 */
    bool median_filter_enabled; /**< 是否启用三点中值去尖峰。 */
} touch_processing_config_t;

/**
 * @brief 触摸 Handler 上下文和状态机状态。
 * @note 该结构包含可变滤波历史，应由 Port 的 operation_mutex 串行访问。
 */
typedef struct
{
    bsp_touch_driver_t driver; /**< 当前装配的具体触摸驱动。 */
    touch_processing_config_t config; /**< 当前映射与滤波配置。 */
    uint16_t history_x[3]; /**< X 轴三点中值历史。 */
    uint16_t history_y[3]; /**< Y 轴三点中值历史。 */
    uint16_t last_x; /**< 最近发布的滤波 X 坐标。 */
    uint16_t last_y; /**< 最近发布的滤波 Y 坐标。 */
    uint16_t jump_x; /**< 待确认大跳点的 X 坐标。 */
    uint16_t jump_y; /**< 待确认大跳点的 Y 坐标。 */
    int32_t filtered_x_q8; /**< X 轴 IIR 状态，Q8 定点像素。 */
    int32_t filtered_y_q8; /**< Y 轴 IIR 状态，Q8 定点像素。 */
    uint32_t last_timestamp_ms; /**< 最近接受触点的单调时钟时间。 */
    uint32_t sequence; /**< 已发布事件的递增序号。 */
    uint8_t history_count; /**< 当前有效中值历史点数，最大为 3。 */
    uint8_t history_index; /**< 下一次中值历史写入位置。 */
    uint8_t press_count; /**< 当前连续有效点计数。 */
    uint8_t release_count; /**< 当前连续丢点计数。 */
    bool initialized; /**< 底层驱动是否已经成功初始化。 */
    bool pressed; /**< Handler 是否处于已确认按下状态。 */
    bool filter_seeded; /**< IIR 状态是否已由首个点初始化。 */
    bool jump_pending; /**< 是否存在等待第二帧确认的大跳点。 */
} bsp_touch_handler_t;

/** @brief 装配触摸驱动，并建立与原始坐标范围一致的默认配置。 */
touch_status_t touch_handler_init(
    bsp_touch_handler_t *handler, const touch_iic_interface_t *i2c,
    const touch_control_interface_t *control,
    const touch_yield_interface_t *yield);
/** @brief 读取、映射并滤波一个触点，生成 DOWN/MOVE/UP 事件。 */
touch_status_t touch_handler_read(bsp_touch_handler_t *handler,
                                  uint32_t timestamp_ms,
                                  touch_point_t *point);
/** @brief 获取 Handler 当前逻辑输出尺寸和最大触点数量。 */
touch_status_t touch_handler_get_info(const bsp_touch_handler_t *handler,
                                      touch_info_t *info);
/** @brief 原子替换处理配置；成功后清空现有触摸状态和滤波历史。 */
touch_status_t touch_handler_set_processing_config(
    bsp_touch_handler_t *handler,
    const touch_processing_config_t *config);
/** @brief 复制当前触摸处理配置。 */
touch_status_t touch_handler_get_processing_config(
    const bsp_touch_handler_t *handler,
    touch_processing_config_t *config);
/** @brief 让底层触摸控制器进入休眠并清除运行状态。 */
touch_status_t touch_handler_sleep(bsp_touch_handler_t *handler);
/** @brief 唤醒触摸控制器并从释放状态重新开始。 */
touch_status_t touch_handler_wakeup(bsp_touch_handler_t *handler);

#endif /* BSP_TOUCH_HANDLER_H */
