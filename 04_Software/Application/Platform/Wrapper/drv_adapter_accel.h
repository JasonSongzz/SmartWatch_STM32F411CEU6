#ifndef DRV_ADAPTER_ACCEL_H
#define DRV_ADAPTER_ACCEL_H

#include <stdbool.h>
#include <stdint.h>

#define ACCEL_DEV_MAX (2U)

/** @brief Platform 层统一使用的三轴浮点向量。 */
typedef struct
{
    float x; /**< X 轴分量。 */
    float y; /**< Y 轴分量。 */
    float z; /**< Z 轴分量。 */
} accel_vector3_t;

/** @brief 一次原子发布的完整 IMU 快照。 */
typedef struct
{
    accel_vector3_t accel_g; /**< 已校准和滤波的加速度，单位 g。 */
    accel_vector3_t gyro_dps; /**< 已校准和滤波的角速度，单位 degree/s。 */
    float temperature_c; /**< 芯片温度，单位摄氏度。 */
    uint32_t timestamp_ms; /**< Port 发布快照时的单调时钟毫秒值。 */
    uint32_t sequence; /**< 每次成功 refresh 后递增，用于识别新数据。 */
} accel_snapshot_t;

/** @brief 与具体传感器型号无关的加速度计校准参数。 */
typedef struct
{
    accel_vector3_t accel_offset_g; /**< 三轴加速度零偏，单位 g。 */
    accel_vector3_t accel_scale; /**< 三轴加速度比例因子。 */
    accel_vector3_t gyro_offset_dps; /**< 三轴角速度零偏，单位 degree/s。 */
} accel_calibration_t;

/** @brief 与具体传感器型号无关的 IMU 滤波配置。 */
typedef struct
{
    uint16_t sample_rate_hz; /**< 生产者实际采样频率，单位 Hz。 */
    float low_pass_cutoff_hz; /**< 一阶低通截止频率，单位 Hz。 */
    bool median3_enabled; /**< 是否在低通之前启用三点中值滤波。 */
} accel_filter_config_t;

/**
 * @brief Wrapper 保存的加速度计设备操作表。
 *
 * Port 注册一份此结构，Service 只能通过下方 drv_adapter_accel_* API
 * 访问设备，从而避免上层直接依赖 BSP 或 MCU/RTOS 实现。
 */
typedef struct accel_drv
{
    uint32_t idx; /**< Wrapper 写入的设备索引。 */
    void *user_data; /**< Port 私有上下文，Wrapper 不解析。 */
    bool (*init)(struct accel_drv *dev); /**< 初始化 Port、Handler 和硬件。 */
    bool (*refresh)(struct accel_drv *dev); /**< 采样并原子发布一个新快照。 */
    bool (*read_cached)(struct accel_drv *dev, float *x, float *y, float *z); /**< 读取缓存加速度，不访问硬件。 */
    bool (*read_snapshot)(struct accel_drv *dev, accel_snapshot_t *snapshot); /**< 原子复制完整缓存快照。 */
    bool (*set_calibration)(struct accel_drv *dev,
                            const accel_calibration_t *calibration); /**< 设置运行时校准。 */
    bool (*get_calibration)(struct accel_drv *dev,
                            accel_calibration_t *calibration); /**< 获取运行时校准。 */
    bool (*set_filter)(struct accel_drv *dev,
                       const accel_filter_config_t *config); /**< 设置运行时滤波参数。 */
    bool (*sleep)(struct accel_drv *dev); /**< 请求设备进入低功耗状态。 */
    bool (*wakeup)(struct accel_drv *dev); /**< 请求设备恢复工作。 */
} accel_drv_t;

/** @brief 注册设备操作表；只复制结构，不访问硬件。 */
bool drv_adapter_accel_reg(uint32_t index, const accel_drv_t *dev);
/** @brief 初始化指定设备。 */
bool drv_adapter_accel_init(uint32_t index);
/** @brief 从硬件采样，并更新 Port 中的原子缓存。 */
bool drv_adapter_accel_refresh(uint32_t index);
/** @brief 使指定设备进入休眠。 */
bool drv_adapter_accel_sleep(uint32_t index);
/** @brief 唤醒指定设备；唤醒后旧缓存失效。 */
bool drv_adapter_accel_wakeup(uint32_t index);
/** @brief 读取缓存中的三轴加速度，不触发新采样。 */
bool drv_adapter_accel_read(uint32_t index, float *x, float *y, float *z);
/** @brief 同步执行 refresh 后再读取三轴加速度。 */
bool drv_adapter_accel_sample(uint32_t index, float *x, float *y, float *z);
/** @brief 原子读取最近发布的完整 IMU 快照。 */
bool drv_adapter_accel_read_snapshot(uint32_t index,
                                     accel_snapshot_t *snapshot);
/** @brief 设置运行时校准；持久化由 Service/Storage 负责。 */
bool drv_adapter_accel_set_calibration(
    uint32_t index, const accel_calibration_t *calibration);
/** @brief 获取当前运行时校准。 */
bool drv_adapter_accel_get_calibration(
    uint32_t index, accel_calibration_t *calibration);
/** @brief 设置运行时滤波配置。 */
bool drv_adapter_accel_set_filter(
    uint32_t index, const accel_filter_config_t *config);

#endif /* DRV_ADAPTER_ACCEL_H */
