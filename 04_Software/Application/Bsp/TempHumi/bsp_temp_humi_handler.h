#ifndef BSP_TEMP_HUMI_HANDLER_H
#define BSP_TEMP_HUMI_HANDLER_H

/* Change only this include when replacing the temperature/humidity sensor. */
#include "bsp_aht21_driver.h"

#include <stdbool.h>
#include <stdint.h>

#define BSP_TEMP_HUMI_MIN_SAMPLES_PER_GROUP (3U)

/**
 * @brief 温湿度线性校准参数。
 *
 * 输出公式为 measured * scale + offset；湿度校准结果最终限制在 0~100%。
 */
typedef struct
{
    float temperature_scale; /**< 温度比例因子，无量纲。 */
    float temperature_offset; /**< 温度偏移，单位摄氏度。 */
    float humidity_scale; /**< 相对湿度比例因子，无量纲。 */
    float humidity_offset; /**< 相对湿度偏移，单位 %RH。 */
} bsp_temp_humi_calibration_t;

/**
 * @brief 温湿度 Handler 上下文。
 *
 * Handler 负责调用具体器件、应用校准、完成去极值平均并缓存最近结果；
 * 该结构由调用方长期持有，并由 Port 的 sample_mutex 串行访问。
 */
typedef struct
{
    bsp_temp_humi_driver_t driver; /**< 当前装配的具体温湿度驱动。 */
    bsp_temp_humi_calibration_t calibration; /**< 当前生效的线性校准参数。 */
    float temperature; /**< 最近一组滤波温度，单位摄氏度。 */
    float humidity; /**< 最近一组滤波相对湿度，单位 %RH。 */
    uint16_t last_sample_count; /**< 最近有效分组包含的原始点数。 */
    bool data_valid; /**< 是否已经生成至少一组有效缓存。 */
    bool initialized; /**< 底层驱动是否已经成功初始化。 */
} bsp_temp_humi_handler_t;

/** @brief 装配底层传感器并建立单位增益、零偏移的默认校准。 */
temp_humi_status_t temp_humi_handler_init(
    bsp_temp_humi_handler_t *handler, temp_humi_iic_interface_t *iic,
    temp_humi_yield_interface_t *yield);
/**
 * @brief 按固定间隔采集一组数据，分别去掉一个最大值和最小值后求均值。
 * @note sample_count 必须不少于 BSP_TEMP_HUMI_MIN_SAMPLES_PER_GROUP；
 *       每个采样点后（包括最后一点）都会让出 sample_interval_ms。
 */
temp_humi_status_t temp_humi_handler_sample_group(
    bsp_temp_humi_handler_t *handler, uint32_t sample_interval_ms,
    uint16_t sample_count, float *temp_c, float *humi_pct);
/** @brief 读取最近一次成功分组的缓存，不触发硬件访问。 */
temp_humi_status_t temp_humi_handler_get_filtered(
    const bsp_temp_humi_handler_t *handler, float *temp_c, float *humi_pct);
/** @brief 更新校准参数；成功后使旧的滤波缓存失效。 */
temp_humi_status_t temp_humi_handler_set_calibration(
    bsp_temp_humi_handler_t *handler,
    const bsp_temp_humi_calibration_t *calibration);
/** @brief 复制当前校准参数到调用方缓冲区。 */
temp_humi_status_t temp_humi_handler_get_calibration(
    const bsp_temp_humi_handler_t *handler,
    bsp_temp_humi_calibration_t *calibration);
/** @brief 让底层器件进入休眠，并把 Handler 标记为未就绪。 */
temp_humi_status_t temp_humi_handler_sleep(bsp_temp_humi_handler_t *handler);
/** @brief 唤醒底层器件，并清除休眠前的缓存数据。 */
temp_humi_status_t temp_humi_handler_wakeup(bsp_temp_humi_handler_t *handler);

#endif /* BSP_TEMP_HUMI_HANDLER_H */
