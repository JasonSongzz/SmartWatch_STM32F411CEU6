#ifndef ACCEL_SERVICE_H
#define ACCEL_SERVICE_H

#include "drv_adapter_accel.h"

#include <stdbool.h>
#include <stdint.h>

#define ACCEL_SERVICE_DEFAULT_DEVICE_INDEX       (0U)
#define ACCEL_SERVICE_DEFAULT_SAMPLE_INTERVAL_MS (10U)
#define ACCEL_SERVICE_DEFAULT_RETRY_INTERVAL_MS  (100U)
#define ACCEL_SERVICE_DEFAULT_CUTOFF_HZ          (15.0f)

#define ACCEL_CALIBRATION_SCALE_MIN              (0.5f)
#define ACCEL_CALIBRATION_SCALE_MAX              (1.5f)
#define ACCEL_CALIBRATION_OFFSET_ABS_MAX_G        (0.5f)
#define ACCEL_GYRO_OFFSET_ABS_MAX_DPS             (100.0f)

typedef struct
{
    uint32_t device_index;
    uint32_t sample_interval_ms;
    uint32_t retry_interval_ms;
    float low_pass_cutoff_hz;
    bool median3_enabled;
} accel_service_config_t;

typedef enum
{
    ACCEL_SERVICE_STATUS_OK = 0,
    ACCEL_SERVICE_STATUS_ERROR_PARAMETER,
    ACCEL_SERVICE_STATUS_NOT_READY,
    ACCEL_SERVICE_STATUS_DRIVER_ERROR,
    ACCEL_SERVICE_STATUS_STORAGE_ERROR,
} accel_service_status_t;

typedef struct
{
    accel_vector3_t positive_x;
    accel_vector3_t negative_x;
    accel_vector3_t positive_y;
    accel_vector3_t negative_y;
    accel_vector3_t positive_z;
    accel_vector3_t negative_z;
} accel_six_position_data_t;

bool accel_service_start(const accel_service_config_t *config);
bool accel_service_read_latest(accel_snapshot_t *snapshot);

accel_service_status_t accel_service_set_calibration(
    const accel_calibration_t *calibration, bool persist);
accel_service_status_t accel_service_get_calibration(
    accel_calibration_t *calibration);
accel_service_status_t accel_service_reset_calibration(bool persist);

/* Inputs are uncalibrated stationary means in g and degrees/second. */
bool accel_service_calculate_six_position_calibration(
    const accel_six_position_data_t *six_position,
    const accel_vector3_t *stationary_gyro_mean_dps,
    accel_calibration_t *calibration);

#endif /* ACCEL_SERVICE_H */
