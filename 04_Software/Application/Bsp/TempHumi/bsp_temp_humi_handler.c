#include "bsp_temp_humi_handler.h"

#include <float.h>
#include <stddef.h>
#include <string.h>

static void temp_humi_filter_reset(bsp_temp_humi_handler_t *handler)
{
    handler->temperature = 0.0f;
    handler->humidity = 0.0f;
    handler->last_sample_count = 0U;
    handler->data_valid = false;
}

static bool temp_humi_sample_is_finite(float sample)
{
    return sample >= -FLT_MAX && sample <= FLT_MAX;
}

static bool temp_humi_calibration_is_valid(
    const bsp_temp_humi_calibration_t *calibration)
{
    return calibration != NULL &&
           temp_humi_sample_is_finite(calibration->temperature_scale) &&
           calibration->temperature_scale > 0.0f &&
           temp_humi_sample_is_finite(calibration->temperature_offset) &&
           temp_humi_sample_is_finite(calibration->humidity_scale) &&
           calibration->humidity_scale > 0.0f &&
           temp_humi_sample_is_finite(calibration->humidity_offset);
}

static void temp_humi_apply_calibration(
    const bsp_temp_humi_calibration_t *calibration,
    float *temperature, float *humidity)
{
    *temperature = *temperature * calibration->temperature_scale +
                   calibration->temperature_offset;
    *humidity = *humidity * calibration->humidity_scale +
                calibration->humidity_offset;

    if (*humidity < 0.0f) *humidity = 0.0f;
    if (*humidity > 100.0f) *humidity = 100.0f;
}

temp_humi_status_t temp_humi_handler_sample_group(
    bsp_temp_humi_handler_t *handler, uint32_t sample_interval_ms,
    uint16_t sample_count, float *temp_c, float *humi_pct)
{
    temp_humi_status_t status;
    float temperature_sum = 0.0f;
    float humidity_sum = 0.0f;
    float temperature_min = 0.0f;
    float temperature_max = 0.0f;
    float humidity_min = 0.0f;
    float humidity_max = 0.0f;
    float temperature;
    float humidity;
    uint16_t index;

    if (handler == NULL || temp_c == NULL || humi_pct == NULL ||
        sample_count < BSP_TEMP_HUMI_MIN_SAMPLES_PER_GROUP)
        return TEMP_HUMI_ERROR_PARAMETER;

    if (!handler->initialized || handler->driver.pf_read_temp_humi == NULL ||
        handler->driver.p_yield_instance == NULL ||
        handler->driver.p_yield_instance->pf_rtos_yield == NULL)
        return TEMP_HUMI_ERROR_RESOURCE;

    for (index = 0U; index < sample_count; ++index) {
        status = handler->driver.pf_read_temp_humi(
            &handler->driver, &temperature, &humidity);
        if (status != TEMP_HUMI_OK) return status;

        if (!temp_humi_sample_is_finite(temperature) ||
            !temp_humi_sample_is_finite(humidity))
            return TEMP_HUMI_ERROR;

        temp_humi_apply_calibration(&handler->calibration,
                                    &temperature, &humidity);
        if (!temp_humi_sample_is_finite(temperature) ||
            !temp_humi_sample_is_finite(humidity))
            return TEMP_HUMI_ERROR;

        if (index == 0U) {
            temperature_min = temperature;
            temperature_max = temperature;
            humidity_min = humidity;
            humidity_max = humidity;
        } else {
            if (temperature < temperature_min) temperature_min = temperature;
            if (temperature > temperature_max) temperature_max = temperature;
            if (humidity < humidity_min) humidity_min = humidity;
            if (humidity > humidity_max) humidity_max = humidity;
        }

        temperature_sum += temperature;
        humidity_sum += humidity;

        /* Also delay after the last point so consecutive groups keep cadence. */
        if (sample_interval_ms > 0U)
            handler->driver.p_yield_instance->pf_rtos_yield(
                sample_interval_ms);
    }

    handler->temperature =
        (temperature_sum - temperature_min - temperature_max) /
        (float)(sample_count - 2U);
    handler->humidity = (humidity_sum - humidity_min - humidity_max) /
                        (float)(sample_count - 2U);
    handler->last_sample_count = sample_count;
    handler->data_valid = true;

    *temp_c = handler->temperature;
    *humi_pct = handler->humidity;
    return TEMP_HUMI_OK;
}

temp_humi_status_t temp_humi_handler_init(
    bsp_temp_humi_handler_t *handler, temp_humi_iic_interface_t *iic,
    temp_humi_yield_interface_t *yield)
{
    temp_humi_status_t status;

    if (handler == NULL || iic == NULL || yield == NULL)
        return TEMP_HUMI_ERROR_PARAMETER;

    memset(handler, 0, sizeof(*handler));
    handler->calibration = (bsp_temp_humi_calibration_t){
        .temperature_scale = 1.0f,
        .temperature_offset = 0.0f,
        .humidity_scale = 1.0f,
        .humidity_offset = 0.0f,
    };
    status = bsp_temp_humi_inst(&handler->driver, iic, yield);
    handler->initialized = status == TEMP_HUMI_OK;
    return status;
}

temp_humi_status_t temp_humi_handler_get_filtered(
    const bsp_temp_humi_handler_t *handler, float *temp_c, float *humi_pct)
{
    if (handler == NULL || temp_c == NULL || humi_pct == NULL)
        return TEMP_HUMI_ERROR_PARAMETER;

    if (!handler->data_valid) return TEMP_HUMI_DATA_NOT_READY;

    *temp_c = handler->temperature;
    *humi_pct = handler->humidity;
    return TEMP_HUMI_OK;
}

temp_humi_status_t temp_humi_handler_set_calibration(
    bsp_temp_humi_handler_t *handler,
    const bsp_temp_humi_calibration_t *calibration)
{
    if (handler == NULL || !temp_humi_calibration_is_valid(calibration))
        return TEMP_HUMI_ERROR_PARAMETER;

    handler->calibration = *calibration;
    temp_humi_filter_reset(handler);
    return TEMP_HUMI_OK;
}

temp_humi_status_t temp_humi_handler_get_calibration(
    const bsp_temp_humi_handler_t *handler,
    bsp_temp_humi_calibration_t *calibration)
{
    if (handler == NULL || calibration == NULL)
        return TEMP_HUMI_ERROR_PARAMETER;

    *calibration = handler->calibration;
    return TEMP_HUMI_OK;
}

temp_humi_status_t temp_humi_handler_sleep(bsp_temp_humi_handler_t *handler)
{
    temp_humi_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL)
        return TEMP_HUMI_ERROR_RESOURCE;

    status = handler->driver.pf_sleep(&handler->driver);
    if (status == TEMP_HUMI_OK) handler->initialized = false;
    return status;
}

temp_humi_status_t temp_humi_handler_wakeup(bsp_temp_humi_handler_t *handler)
{
    temp_humi_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return TEMP_HUMI_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);
    if (status == TEMP_HUMI_OK) {
        temp_humi_filter_reset(handler);
        handler->initialized = true;
    }
    return status;
}
