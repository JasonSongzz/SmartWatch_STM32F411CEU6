#include "temp_humi_service.h"

#include "osal.h"
#include "storage_service.h"

#include <float.h>
#include <stddef.h>

#define TEMP_HUMI_SERVICE_TASK_STACK_SIZE_BYTES (1024U)
#define TEMP_HUMI_CALIBRATION_STORAGE_KEY "sensor/temp_humi/calibration"
#define TEMP_HUMI_CALIBRATION_VERSION     (1U)

typedef struct {
    uint16_t version;
    uint16_t size;
    temp_humi_service_calibration_t calibration;
} temp_humi_calibration_record_t;

_Static_assert(sizeof(temp_humi_calibration_record_t) <=
               STORAGE_CONFIG_MAX_DATA_SIZE,
               "Temperature/humidity calibration exceeds storage limit");

typedef struct {
    temp_humi_service_config_t config;
    osal_task_handle_t task_handle;
    osal_mutex_handle_t state_mutex;
    bool calibration_ready;
    bool started;
} temp_humi_service_context_t;

static temp_humi_service_context_t s_service;

static const temp_humi_service_config_t s_default_config = {
    .device_index = TEMP_HUMI_SERVICE_DEFAULT_DEVICE_INDEX,
    .sample_interval_ms = TEMP_HUMI_SERVICE_DEFAULT_SAMPLE_INTERVAL_MS,
    .retry_interval_ms = TEMP_HUMI_SERVICE_DEFAULT_RETRY_INTERVAL_MS,
    .samples_per_group = TEMP_HUMI_SERVICE_DEFAULT_SAMPLES_PER_GROUP,
};

static const temp_humi_service_calibration_t s_default_calibration = {
    .temperature_scale = 1.0f,
    .temperature_offset = 0.0f,
    .humidity_scale = 1.0f,
    .humidity_offset = 0.0f,
};

static bool temp_humi_value_is_finite(float value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

static bool temp_humi_calibration_is_valid(
    const temp_humi_service_calibration_t *calibration)
{
    return calibration != NULL &&
           temp_humi_value_is_finite(calibration->temperature_scale) &&
           calibration->temperature_scale >=
               TEMP_HUMI_CALIBRATION_SCALE_MIN &&
           calibration->temperature_scale <=
               TEMP_HUMI_CALIBRATION_SCALE_MAX &&
           temp_humi_value_is_finite(calibration->temperature_offset) &&
           calibration->temperature_offset >=
               -TEMP_HUMI_TEMPERATURE_OFFSET_ABS_MAX &&
           calibration->temperature_offset <=
               TEMP_HUMI_TEMPERATURE_OFFSET_ABS_MAX &&
           temp_humi_value_is_finite(calibration->humidity_scale) &&
           calibration->humidity_scale >= TEMP_HUMI_CALIBRATION_SCALE_MIN &&
           calibration->humidity_scale <= TEMP_HUMI_CALIBRATION_SCALE_MAX &&
           temp_humi_value_is_finite(calibration->humidity_offset) &&
           calibration->humidity_offset >=
               -TEMP_HUMI_HUMIDITY_OFFSET_ABS_MAX &&
           calibration->humidity_offset <=
               TEMP_HUMI_HUMIDITY_OFFSET_ABS_MAX;
}

static temp_humi_service_status_t temp_humi_service_load_calibration(
    temp_humi_service_calibration_t *calibration)
{
    temp_humi_calibration_record_t record = {0};
    storage_status_t storage_status;
    size_t size = sizeof(record);

    if (calibration == NULL)
        return TEMP_HUMI_SERVICE_STATUS_ERROR_PARAMETER;

    *calibration = s_default_calibration;
    if (!storage_service_is_ready())
        return TEMP_HUMI_SERVICE_STATUS_NOT_READY;

    storage_status = storage_config_load(
        TEMP_HUMI_CALIBRATION_STORAGE_KEY, &record, &size);
    if (storage_status == STORAGE_STATUS_NOT_FOUND)
        return TEMP_HUMI_SERVICE_STATUS_OK;
    if (storage_status != STORAGE_STATUS_OK &&
        storage_status != STORAGE_STATUS_DEGRADED)
        return TEMP_HUMI_SERVICE_STATUS_STORAGE_ERROR;

    if (size != sizeof(record) ||
        record.version != TEMP_HUMI_CALIBRATION_VERSION ||
        record.size != sizeof(record) ||
        !temp_humi_calibration_is_valid(&record.calibration))
        return TEMP_HUMI_SERVICE_STATUS_STORAGE_ERROR;

    *calibration = record.calibration;
    return TEMP_HUMI_SERVICE_STATUS_OK;
}

static temp_humi_service_status_t temp_humi_service_save_calibration(
    const temp_humi_service_calibration_t *calibration)
{
    const temp_humi_calibration_record_t record = {
        .version = TEMP_HUMI_CALIBRATION_VERSION,
        .size = (uint16_t)sizeof(temp_humi_calibration_record_t),
        .calibration = *calibration,
    };
    storage_status_t status;

    if (!storage_service_is_ready())
        return TEMP_HUMI_SERVICE_STATUS_NOT_READY;

    status = storage_config_save(TEMP_HUMI_CALIBRATION_STORAGE_KEY,
                                 &record, sizeof(record));
    return status == STORAGE_STATUS_OK || status == STORAGE_STATUS_DEGRADED
         ? TEMP_HUMI_SERVICE_STATUS_OK
         : TEMP_HUMI_SERVICE_STATUS_STORAGE_ERROR;
}

static bool temp_humi_service_lock_calibration_state(void)
{
    if (!s_service.started || s_service.state_mutex == NULL ||
        osal_mutex_take(s_service.state_mutex,
                        OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return false;

    if (!s_service.calibration_ready) {
        (void)osal_mutex_give(s_service.state_mutex);
        return false;
    }

    return true;
}

static void temp_humi_service_task(void *argument)
{
    temphumi_sample_config_t sampling;
    temp_humi_service_calibration_t calibration;
    float temperature;
    float humidity;
    bool sensor_initialized = false;
    bool calibration_applied = false;

    (void)argument;
    sampling.sample_interval_ms = s_service.config.sample_interval_ms;
    sampling.samples_per_group = s_service.config.samples_per_group;
    (void)temp_humi_service_load_calibration(&calibration);

    for (;;) {
        if (!sensor_initialized) {
            sensor_initialized = drv_adapter_temphumi_init(
                s_service.config.device_index);
            if (!sensor_initialized) {
                osal_task_delay_ms(s_service.config.retry_interval_ms);
                continue;
            }
        }

        if (!calibration_applied) {
            calibration_applied = drv_adapter_temphumi_set_calibration(
                s_service.config.device_index, &calibration);
            if (!calibration_applied) {
                osal_task_delay_ms(s_service.config.retry_interval_ms);
                continue;
            }

            if (osal_mutex_take(s_service.state_mutex,
                                OSAL_WAIT_FOREVER) == OSAL_SUCCESS) {
                s_service.calibration_ready = true;
                (void)osal_mutex_give(s_service.state_mutex);
            }
        }

        if (!drv_adapter_temphumi_sample_group(
                s_service.config.device_index, &sampling,
                &temperature, &humidity)) {
            osal_task_delay_ms(s_service.config.retry_interval_ms);
        }
    }
}

bool temp_humi_service_start(const temp_humi_service_config_t *config)
{
    temp_humi_service_config_t selected = config != NULL
                                        ? *config : s_default_config;

    if (s_service.started ||
        selected.device_index >= TEMP_HUMI_DEV_MAX ||
        selected.samples_per_group < TEMP_HUMI_MIN_SAMPLES_PER_GROUP)
        return false;

    if (selected.retry_interval_ms == 0U)
        selected.retry_interval_ms = TEMP_HUMI_SERVICE_DEFAULT_RETRY_INTERVAL_MS;

    s_service.config = selected;
    s_service.calibration_ready = false;
    if (osal_mutex_create(&s_service.state_mutex) != OSAL_SUCCESS)
        return false;

    if (osal_task_create("temp_humi", temp_humi_service_task,
                         TEMP_HUMI_SERVICE_TASK_STACK_SIZE_BYTES,
                         OSAL_PRIORITY_LOW, &s_service.task_handle,
                         NULL) != OSAL_SUCCESS) {
        osal_mutex_delete(s_service.state_mutex);
        s_service.state_mutex = NULL;
        return false;
    }

    s_service.started = true;
    return true;
}

bool temp_humi_service_read_latest(float *temperature, float *humidity)
{
    if (!s_service.started || temperature == NULL || humidity == NULL)
        return false;

    return drv_adapter_temphumi_read_temp_and_humi(
        s_service.config.device_index, temperature, humidity);
}

temp_humi_service_status_t temp_humi_service_set_calibration(
    const temp_humi_service_calibration_t *calibration, bool persist)
{
    temp_humi_service_status_t status = TEMP_HUMI_SERVICE_STATUS_OK;

    if (!temp_humi_calibration_is_valid(calibration))
        return TEMP_HUMI_SERVICE_STATUS_ERROR_PARAMETER;
    if (!temp_humi_service_lock_calibration_state())
        return TEMP_HUMI_SERVICE_STATUS_NOT_READY;

    if (!drv_adapter_temphumi_set_calibration(
            s_service.config.device_index, calibration)) {
        status = TEMP_HUMI_SERVICE_STATUS_DRIVER_ERROR;
    } else if (persist) {
        status = temp_humi_service_save_calibration(calibration);
    }

    if (osal_mutex_give(s_service.state_mutex) != OSAL_SUCCESS &&
        status == TEMP_HUMI_SERVICE_STATUS_OK)
        status = TEMP_HUMI_SERVICE_STATUS_DRIVER_ERROR;
    return status;
}

temp_humi_service_status_t temp_humi_service_get_calibration(
    temp_humi_service_calibration_t *calibration)
{
    temp_humi_service_status_t status;

    if (calibration == NULL)
        return TEMP_HUMI_SERVICE_STATUS_ERROR_PARAMETER;
    if (!temp_humi_service_lock_calibration_state())
        return TEMP_HUMI_SERVICE_STATUS_NOT_READY;

    status = drv_adapter_temphumi_get_calibration(
                 s_service.config.device_index, calibration)
           ? TEMP_HUMI_SERVICE_STATUS_OK
           : TEMP_HUMI_SERVICE_STATUS_DRIVER_ERROR;
    if (osal_mutex_give(s_service.state_mutex) != OSAL_SUCCESS &&
        status == TEMP_HUMI_SERVICE_STATUS_OK)
        status = TEMP_HUMI_SERVICE_STATUS_DRIVER_ERROR;
    return status;
}

temp_humi_service_status_t temp_humi_service_reset_calibration(bool persist)
{
    return temp_humi_service_set_calibration(
        &s_default_calibration, persist);
}

bool temp_humi_service_calculate_two_point_calibration(
    const temp_humi_calibration_point_t temperature_points[2],
    const temp_humi_calibration_point_t humidity_points[2],
    temp_humi_service_calibration_t *calibration)
{
    temp_humi_service_calibration_t candidate;
    float measured_delta;

    if (temperature_points == NULL || humidity_points == NULL ||
        calibration == NULL)
        return false;

    measured_delta = temperature_points[1].measured -
                     temperature_points[0].measured;
    if (!temp_humi_value_is_finite(measured_delta) ||
        (measured_delta >= -FLT_EPSILON && measured_delta <= FLT_EPSILON))
        return false;
    candidate.temperature_scale =
        (temperature_points[1].reference -
         temperature_points[0].reference) / measured_delta;
    candidate.temperature_offset = temperature_points[0].reference -
        candidate.temperature_scale * temperature_points[0].measured;

    measured_delta = humidity_points[1].measured -
                     humidity_points[0].measured;
    if (!temp_humi_value_is_finite(measured_delta) ||
        (measured_delta >= -FLT_EPSILON && measured_delta <= FLT_EPSILON))
        return false;
    candidate.humidity_scale =
        (humidity_points[1].reference -
         humidity_points[0].reference) / measured_delta;
    candidate.humidity_offset = humidity_points[0].reference -
        candidate.humidity_scale * humidity_points[0].measured;

    if (!temp_humi_calibration_is_valid(&candidate)) return false;

    *calibration = candidate;
    return true;
}
