#ifndef BSP_ACCEL_HANDLER_H
#define BSP_ACCEL_HANDLER_H

/* Change only this include when replacing the accelerometer. */
#include "bsp_mpu6050_driver.h"

#include <stdbool.h>
#include <stdint.h>

#define BSP_ACCEL_FILTER_CHANNEL_COUNT (6U)
#define BSP_ACCEL_MEDIAN_WINDOW_SIZE   (3U)

/** @brief 加速度计和陀螺仪的运行时校准参数。 */
typedef struct
{
    accel_data_t accel_offset_g; /**< 三轴零偏，单位 g，先从原始测量值中减去。 */
    accel_data_t accel_scale; /**< 三轴比例因子，在去零偏后相乘。 */
    accel_data_t gyro_offset_dps; /**< 静止角速度零偏，单位 degree/s。 */
} bsp_accel_calibration_t;

/** @brief 六通道 IMU 滤波器配置。 */
typedef struct
{
    uint16_t sample_rate_hz; /**< Handler 实际调用频率，单位 Hz。 */
    float low_pass_cutoff_hz; /**< 一阶低通截止频率，必须小于奈奎斯特频率。 */
    bool median3_enabled; /**< 是否在低通前启用三点中值去尖峰。 */
} bsp_accel_filter_config_t;

/**
 * @brief 加速度计 Handler 上下文。
 *
 * Handler 位于具体器件驱动之上，负责校准、滤波及生命周期管理；该结构
 * 由调用方长期持有，并由 Port 的 sample_mutex 串行访问。
 */
typedef struct
{
    bsp_accel_driver_t driver; /**< 当前装配的具体传感器驱动。 */
    bsp_accel_calibration_t calibration; /**< 当前生效的校准参数。 */
    bsp_accel_filter_config_t filter_config; /**< 当前生效的滤波参数。 */
    float median_history[BSP_ACCEL_FILTER_CHANNEL_COUNT]
                        [BSP_ACCEL_MEDIAN_WINDOW_SIZE]; /**< 六通道三点历史。 */
    float low_pass_state[BSP_ACCEL_FILTER_CHANNEL_COUNT]; /**< 六通道 IIR 状态。 */
    uint8_t median_index; /**< 下一次写入中值历史的位置。 */
    uint8_t median_count; /**< 已积累的有效历史点数量，最大为 3。 */
    bool filter_initialized; /**< 一阶低通状态是否已由首帧初始化。 */
    bool initialized; /**< 底层驱动是否已经成功初始化。 */
} bsp_accel_handler_t;

/** @brief 装配底层传感器并建立默认校准和滤波配置。 */
accel_status_t accel_handler_init(bsp_accel_handler_t *handler,
                                  accel_iic_interface_t *iic,
                                  accel_yield_interface_t *yield);
/** @brief 读取一次经校准、滤波后的三轴加速度，单位 g。 */
accel_status_t accel_handler_read(bsp_accel_handler_t *handler,
                                  accel_data_t *accel);
/** @brief 原子读取并处理加速度、角速度和芯片温度。 */
accel_status_t accel_handler_read_imu(bsp_accel_handler_t *handler,
                                      accel_imu_data_t *imu);
/** @brief 更新运行时校准参数；成功后会清空旧滤波状态。 */
accel_status_t accel_handler_set_calibration(
    bsp_accel_handler_t *handler,
    const bsp_accel_calibration_t *calibration);
/** @brief 复制当前校准参数到调用方缓冲区。 */
accel_status_t accel_handler_get_calibration(
    const bsp_accel_handler_t *handler,
    bsp_accel_calibration_t *calibration);
/** @brief 更新滤波参数；成功后会清空旧滤波状态。 */
accel_status_t accel_handler_set_filter(
    bsp_accel_handler_t *handler,
    const bsp_accel_filter_config_t *config);
/** @brief 清空中值历史和低通状态，不改变滤波配置。 */
void accel_handler_reset_filter(bsp_accel_handler_t *handler);
/** @brief 让底层器件进入休眠，并把 Handler 标记为未就绪。 */
accel_status_t accel_handler_sleep(bsp_accel_handler_t *handler);
/** @brief 唤醒底层器件并清空休眠前的滤波历史。 */
accel_status_t accel_handler_wakeup(bsp_accel_handler_t *handler);

#endif /* BSP_ACCEL_HANDLER_H */
