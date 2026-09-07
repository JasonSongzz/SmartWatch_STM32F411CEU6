#ifndef TEMP_HUMI_SERVICE_H
#define TEMP_HUMI_SERVICE_H

#include "drv_adapter_temphumi.h"

#include <stdbool.h>
#include <stdint.h>

#define TEMP_HUMI_SERVICE_DEFAULT_DEVICE_INDEX       (0U)
#define TEMP_HUMI_SERVICE_DEFAULT_SAMPLE_INTERVAL_MS (200U)
#define TEMP_HUMI_SERVICE_DEFAULT_SAMPLES_PER_GROUP  (5U)
#define TEMP_HUMI_SERVICE_DEFAULT_RETRY_INTERVAL_MS  (1000U)

#define TEMP_HUMI_CALIBRATION_SCALE_MIN              (0.5f)
#define TEMP_HUMI_CALIBRATION_SCALE_MAX              (1.5f)
#define TEMP_HUMI_TEMPERATURE_OFFSET_ABS_MAX          (20.0f)
#define TEMP_HUMI_HUMIDITY_OFFSET_ABS_MAX             (30.0f)

typedef struct {
    uint32_t device_index;
    uint32_t sample_interval_ms;
    uint32_t retry_interval_ms;
    uint16_t samples_per_group;
} temp_humi_service_config_t;

typedef temphumi_calibration_t temp_humi_service_calibration_t;

typedef struct {
    float measured;
    float reference;
} temp_humi_calibration_point_t;

typedef enum {
    TEMP_HUMI_SERVICE_STATUS_OK = 0,
    TEMP_HUMI_SERVICE_STATUS_ERROR_PARAMETER,
    TEMP_HUMI_SERVICE_STATUS_NOT_READY,
    TEMP_HUMI_SERVICE_STATUS_DRIVER_ERROR,
    TEMP_HUMI_SERVICE_STATUS_STORAGE_ERROR,
} temp_humi_service_status_t;

/* Pass NULL to use the defaults above. The configuration is copied. */
bool temp_humi_service_start(const temp_humi_service_config_t *config);

/* Returns false until the first complete filtered group is available. */
bool temp_humi_service_read_latest(float *temperature, float *humidity);

/* Runtime calibration is applied first; a later persistence error is reported. */
temp_humi_service_status_t temp_humi_service_set_calibration(
    const temp_humi_service_calibration_t *calibration, bool persist);
temp_humi_service_status_t temp_humi_service_get_calibration(
    temp_humi_service_calibration_t *calibration);
temp_humi_service_status_t temp_humi_service_reset_calibration(bool persist);

/* Build independent temperature/humidity scale and offset from two points. */
bool temp_humi_service_calculate_two_point_calibration(
    const temp_humi_calibration_point_t temperature_points[2],
    const temp_humi_calibration_point_t humidity_points[2],
    temp_humi_service_calibration_t *calibration);

#endif /* TEMP_HUMI_SERVICE_H */
