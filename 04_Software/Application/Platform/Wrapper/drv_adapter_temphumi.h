#ifndef DRV_ADAPTER_TEMPHUMI_H
#define DRV_ADAPTER_TEMPHUMI_H

#include <stdbool.h>
#include <stdint.h>

#define TEMP_HUMI_DEV_MAX (2U)
#define TEMP_HUMI_MIN_SAMPLES_PER_GROUP (3U)

/** @brief 一组温湿度数据的采样节奏配置。 */
typedef struct {
    uint32_t sample_interval_ms; /**< 相邻采样点的 OSAL 延时间隔，单位 ms。 */
    uint16_t samples_per_group; /**< 每组原始点数，必须不少于 3。 */
} temphumi_sample_config_t;

/**
 * @brief 与具体温湿度传感器型号无关的线性校准参数。
 * @note 输出公式为 measured * scale + offset。
 */
typedef struct {
    float temperature_scale; /**< 温度比例因子，无量纲。 */
    float temperature_offset; /**< 温度偏移，单位摄氏度。 */
    float humidity_scale; /**< 相对湿度比例因子，无量纲。 */
    float humidity_offset; /**< 相对湿度偏移，单位 %RH。 */
} temphumi_calibration_t;

/**
 * @brief Wrapper 保存的温湿度设备操作表。
 *
 * Port 注册该操作表，Service 通过 drv_adapter_temphumi_* API 使用设备，
 * 因此不会直接依赖 AHT21、SHT30、STM32 HAL 或具体 RTOS。
 */
typedef struct temphumi_drv {
    uint32_t idx; /**< Wrapper 写入的设备索引。 */
    void *user_data; /**< Port 私有上下文，Wrapper 不解析。 */
    bool (*init)(struct temphumi_drv *dev); /**< 初始化 Port、Handler 和硬件。 */
    bool (*refresh)(struct temphumi_drv *dev); /**< 使用默认配置采集并发布一组数据。 */
    bool (*sample_group)(struct temphumi_drv *dev,
                         const temphumi_sample_config_t *config); /**< 使用指定配置采集并发布一组数据。 */
    bool (*read_cached)(struct temphumi_drv *dev, float *temp, float *humi); /**< 读取缓存，不访问硬件。 */
    bool (*set_calibration)(struct temphumi_drv *dev,
                            const temphumi_calibration_t *calibration); /**< 设置运行时校准。 */
    bool (*get_calibration)(struct temphumi_drv *dev,
                            temphumi_calibration_t *calibration); /**< 获取运行时校准。 */
    bool (*sleep)(struct temphumi_drv *dev); /**< 请求设备进入低功耗状态。 */
    bool (*wakeup)(struct temphumi_drv *dev); /**< 请求设备恢复工作。 */
} temphumi_drv_t;

/** @brief 注册设备操作表；只复制结构，不访问硬件。 */
bool drv_adapter_temphumi_reg(uint32_t index, const temphumi_drv_t *dev);
/** @brief 初始化指定温湿度设备。 */
bool drv_adapter_temphumi_init(uint32_t index);
/** @brief 使用注册时的默认分组参数完成一次采集并更新缓存。 */
bool drv_adapter_temphumi_refresh(uint32_t index);
/** @brief 读取最近一组温度和湿度缓存，不触发硬件采样。 */
bool drv_adapter_temphumi_read_temp_and_humi(uint32_t index,
                                             float *temperature,
                                             float *humidity);
/** @brief 同步执行一次默认分组采样，再返回新结果。 */
bool drv_adapter_temphumi_sample(uint32_t index,
                                 float *temperature, float *humidity);
/** @brief 使用指定采样间隔和点数同步采样，再返回去极值平均结果。 */
bool drv_adapter_temphumi_sample_group(
    uint32_t index, const temphumi_sample_config_t *config,
    float *temperature, float *humidity);
/** @brief 设置运行时校准；持久化由 Service/Storage 负责。 */
bool drv_adapter_temphumi_set_calibration(
    uint32_t index, const temphumi_calibration_t *calibration);
/** @brief 获取当前运行时校准。 */
bool drv_adapter_temphumi_get_calibration(
    uint32_t index, temphumi_calibration_t *calibration);
/** @brief 使指定设备进入休眠。 */
bool drv_adapter_temphumi_sleep(uint32_t index);
/** @brief 唤醒指定设备；唤醒后旧缓存失效。 */
bool drv_adapter_temphumi_wakeup(uint32_t index);

#endif /* DRV_ADAPTER_TEMP_HUMI_H */
