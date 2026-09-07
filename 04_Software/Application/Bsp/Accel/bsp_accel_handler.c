#include "bsp_accel_handler.h"

#include <float.h>
#include <stddef.h>
#include <string.h>

#define BSP_ACCEL_DEFAULT_SAMPLE_RATE_HZ (100U)
#define BSP_ACCEL_DEFAULT_CUTOFF_HZ      (15.0f)
#define BSP_ACCEL_TWO_PI                 (6.28318530718f)

static bool accel_value_is_finite(float value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

static bool accel_vector_is_finite(const accel_data_t *vector)
{
    return vector != NULL && accel_value_is_finite(vector->x) &&
           accel_value_is_finite(vector->y) &&
           accel_value_is_finite(vector->z);
}

static bool accel_calibration_is_valid(
    const bsp_accel_calibration_t *calibration)
{
    return calibration != NULL &&
           accel_vector_is_finite(&calibration->accel_offset_g) &&
           accel_vector_is_finite(&calibration->accel_scale) &&
           calibration->accel_scale.x > 0.0f &&
           calibration->accel_scale.y > 0.0f &&
           calibration->accel_scale.z > 0.0f &&
           accel_vector_is_finite(&calibration->gyro_offset_dps);
}

static bool accel_filter_is_valid(const bsp_accel_filter_config_t *config)
{
    return config != NULL && config->sample_rate_hz > 0U &&
           accel_value_is_finite(config->low_pass_cutoff_hz) &&
           config->low_pass_cutoff_hz > 0.0f &&
           config->low_pass_cutoff_hz < (float)config->sample_rate_hz * 0.5f;
}

static float accel_median3(float a, float b, float c)
{
    if (a > b) { float temporary = a; a = b; b = temporary; }
    if (b > c) { float temporary = b; b = c; c = temporary; }
    if (a > b) { float temporary = a; a = b; b = temporary; }
    return b;
}

void accel_handler_reset_filter(bsp_accel_handler_t *handler)
{
    if (handler == NULL) return;

    memset(handler->median_history, 0, sizeof(handler->median_history));
    memset(handler->low_pass_state, 0, sizeof(handler->low_pass_state));
    handler->median_index = 0U;
    handler->median_count = 0U;
    handler->filter_initialized = false;
}

static void accel_apply_calibration(
    const bsp_accel_calibration_t *calibration, accel_imu_data_t *imu)
{
    imu->accel_g.x = (imu->accel_g.x - calibration->accel_offset_g.x) *
                     calibration->accel_scale.x;
    imu->accel_g.y = (imu->accel_g.y - calibration->accel_offset_g.y) *
                     calibration->accel_scale.y;
    imu->accel_g.z = (imu->accel_g.z - calibration->accel_offset_g.z) *
                     calibration->accel_scale.z;
    imu->gyro_dps.x -= calibration->gyro_offset_dps.x;
    imu->gyro_dps.y -= calibration->gyro_offset_dps.y;
    imu->gyro_dps.z -= calibration->gyro_offset_dps.z;
}

static void accel_filter_sample(bsp_accel_handler_t *handler,
                                accel_imu_data_t *imu)
{
    float values[BSP_ACCEL_FILTER_CHANNEL_COUNT] = {
        imu->accel_g.x, imu->accel_g.y, imu->accel_g.z,
        imu->gyro_dps.x, imu->gyro_dps.y, imu->gyro_dps.z,
    };
    float angular_frequency = BSP_ACCEL_TWO_PI *
                              handler->filter_config.low_pass_cutoff_hz;
    float alpha = angular_frequency /
                  ((float)handler->filter_config.sample_rate_hz +
                   angular_frequency);
    uint8_t channel;

    for (channel = 0U; channel < BSP_ACCEL_FILTER_CHANNEL_COUNT; ++channel) {
        handler->median_history[channel][handler->median_index] =
            values[channel];
        if (handler->filter_config.median3_enabled &&
            handler->median_count >= BSP_ACCEL_MEDIAN_WINDOW_SIZE - 1U) {
            values[channel] = accel_median3(
                handler->median_history[channel][0],
                handler->median_history[channel][1],
                handler->median_history[channel][2]);
        }

        if (!handler->filter_initialized) {
            handler->low_pass_state[channel] = values[channel];
        } else {
            handler->low_pass_state[channel] +=
                alpha * (values[channel] - handler->low_pass_state[channel]);
        }
        values[channel] = handler->low_pass_state[channel];
    }

    handler->median_index = (uint8_t)((handler->median_index + 1U) %
                                     BSP_ACCEL_MEDIAN_WINDOW_SIZE);
    if (handler->median_count < BSP_ACCEL_MEDIAN_WINDOW_SIZE)
        ++handler->median_count;
    handler->filter_initialized = true;

    imu->accel_g.x = values[0];
    imu->accel_g.y = values[1];
    imu->accel_g.z = values[2];
    imu->gyro_dps.x = values[3];
    imu->gyro_dps.y = values[4];
    imu->gyro_dps.z = values[5];
}

accel_status_t accel_handler_init(bsp_accel_handler_t *handler,
                                  accel_iic_interface_t *iic,
                                  accel_yield_interface_t *yield)
{
    accel_status_t status;

    if (handler == NULL || iic == NULL || yield == NULL)
        return ACCEL_ERROR_PARAMETER;

    memset(handler, 0, sizeof(*handler));
    handler->calibration.accel_scale = (accel_data_t){1.0f, 1.0f, 1.0f};
    handler->filter_config = (bsp_accel_filter_config_t){
        .sample_rate_hz = BSP_ACCEL_DEFAULT_SAMPLE_RATE_HZ,
        .low_pass_cutoff_hz = BSP_ACCEL_DEFAULT_CUTOFF_HZ,
        .median3_enabled = true,
    };
    status = bsp_accel_inst(&handler->driver, iic, yield);
    handler->initialized = status == ACCEL_OK;
    return status;
}

accel_status_t accel_handler_read_imu(bsp_accel_handler_t *handler,
                                      accel_imu_data_t *imu)
{
    accel_status_t status;

    if (handler == NULL || imu == NULL || !handler->initialized ||
        handler->driver.pf_read_imu == NULL)
        return ACCEL_ERROR_RESOURCE;

    status = handler->driver.pf_read_imu(&handler->driver, imu);
    if (status != ACCEL_OK) return status;
    if (!accel_vector_is_finite(&imu->accel_g) ||
        !accel_vector_is_finite(&imu->gyro_dps) ||
        !accel_value_is_finite(imu->temperature_c))
        return ACCEL_ERROR;

    accel_apply_calibration(&handler->calibration, imu);
    accel_filter_sample(handler, imu);
    return ACCEL_OK;
}

accel_status_t accel_handler_read(bsp_accel_handler_t *handler,
                                  accel_data_t *accel)
{
    accel_imu_data_t imu;
    accel_status_t status;

    if (accel == NULL) return ACCEL_ERROR_PARAMETER;
    status = accel_handler_read_imu(handler, &imu);
    if (status == ACCEL_OK) *accel = imu.accel_g;
    return status;
}

accel_status_t accel_handler_set_calibration(
    bsp_accel_handler_t *handler,
    const bsp_accel_calibration_t *calibration)
{
    if (handler == NULL || !accel_calibration_is_valid(calibration))
        return ACCEL_ERROR_PARAMETER;

    handler->calibration = *calibration;
    accel_handler_reset_filter(handler);
    return ACCEL_OK;
}

accel_status_t accel_handler_get_calibration(
    const bsp_accel_handler_t *handler,
    bsp_accel_calibration_t *calibration)
{
    if (handler == NULL || calibration == NULL)
        return ACCEL_ERROR_PARAMETER;

    *calibration = handler->calibration;
    return ACCEL_OK;
}

accel_status_t accel_handler_set_filter(
    bsp_accel_handler_t *handler,
    const bsp_accel_filter_config_t *config)
{
    if (handler == NULL || !accel_filter_is_valid(config))
        return ACCEL_ERROR_PARAMETER;

    handler->filter_config = *config;
    accel_handler_reset_filter(handler);
    return ACCEL_OK;
}

accel_status_t accel_handler_sleep(bsp_accel_handler_t *handler)
{
    accel_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL)
        return ACCEL_ERROR_RESOURCE;

    status = handler->driver.pf_sleep(&handler->driver);
    if (status == ACCEL_OK) handler->initialized = false;
    return status;
}

accel_status_t accel_handler_wakeup(bsp_accel_handler_t *handler)
{
    accel_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return ACCEL_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);
    if (status == ACCEL_OK) {
        accel_handler_reset_filter(handler);
        handler->initialized = true;
    }
    return status;
}
