#include "accel_service.h"

#include "osal.h"
#include "storage_service.h"

#include <float.h>
#include <stddef.h>

#define ACCEL_SERVICE_TASK_STACK_SIZE_BYTES (1280U)
#define ACCEL_CALIBRATION_STORAGE_KEY "sensor/accel/calibration"
#define ACCEL_CALIBRATION_VERSION     (1U)

typedef struct
{
    uint16_t version;
    uint16_t size;
    accel_calibration_t calibration;
} accel_calibration_record_t;

_Static_assert(sizeof(accel_calibration_record_t) <=
               STORAGE_CONFIG_MAX_DATA_SIZE,
               "Accelerometer calibration exceeds storage limit");

typedef struct
{
    accel_service_config_t config;
    osal_task_handle_t task_handle;
    osal_mutex_handle_t state_mutex;
    bool calibration_ready;
    bool started;
} accel_service_context_t;

static accel_service_context_t s_service;

static const accel_service_config_t s_default_config = {
    .device_index = ACCEL_SERVICE_DEFAULT_DEVICE_INDEX,
    .sample_interval_ms = ACCEL_SERVICE_DEFAULT_SAMPLE_INTERVAL_MS,
    .retry_interval_ms = ACCEL_SERVICE_DEFAULT_RETRY_INTERVAL_MS,
    .low_pass_cutoff_hz = ACCEL_SERVICE_DEFAULT_CUTOFF_HZ,
    .median3_enabled = true,
};

static const accel_calibration_t s_default_calibration = {
    .accel_offset_g = {0.0f, 0.0f, 0.0f},
    .accel_scale = {1.0f, 1.0f, 1.0f},
    .gyro_offset_dps = {0.0f, 0.0f, 0.0f},
};

static float accel_abs(float value)
{
    return value < 0.0f ? -value : value;
}

static bool accel_value_is_finite(float value)
{
    return value >= -FLT_MAX && value <= FLT_MAX;
}

static bool accel_vector_is_finite(const accel_vector3_t *vector)
{
    return vector != NULL && accel_value_is_finite(vector->x) &&
           accel_value_is_finite(vector->y) &&
           accel_value_is_finite(vector->z);
}

static bool accel_calibration_is_valid(
    const accel_calibration_t *calibration)
{
    return calibration != NULL &&
           accel_vector_is_finite(&calibration->accel_offset_g) &&
           accel_abs(calibration->accel_offset_g.x) <=
               ACCEL_CALIBRATION_OFFSET_ABS_MAX_G &&
           accel_abs(calibration->accel_offset_g.y) <=
               ACCEL_CALIBRATION_OFFSET_ABS_MAX_G &&
           accel_abs(calibration->accel_offset_g.z) <=
               ACCEL_CALIBRATION_OFFSET_ABS_MAX_G &&
           accel_vector_is_finite(&calibration->accel_scale) &&
           calibration->accel_scale.x >= ACCEL_CALIBRATION_SCALE_MIN &&
           calibration->accel_scale.x <= ACCEL_CALIBRATION_SCALE_MAX &&
           calibration->accel_scale.y >= ACCEL_CALIBRATION_SCALE_MIN &&
           calibration->accel_scale.y <= ACCEL_CALIBRATION_SCALE_MAX &&
           calibration->accel_scale.z >= ACCEL_CALIBRATION_SCALE_MIN &&
           calibration->accel_scale.z <= ACCEL_CALIBRATION_SCALE_MAX &&
           accel_vector_is_finite(&calibration->gyro_offset_dps) &&
           accel_abs(calibration->gyro_offset_dps.x) <=
               ACCEL_GYRO_OFFSET_ABS_MAX_DPS &&
           accel_abs(calibration->gyro_offset_dps.y) <=
               ACCEL_GYRO_OFFSET_ABS_MAX_DPS &&
           accel_abs(calibration->gyro_offset_dps.z) <=
               ACCEL_GYRO_OFFSET_ABS_MAX_DPS;
}

static bool accel_service_config_is_valid(
    const accel_service_config_t *config)
{
    uint32_t sample_rate_hz;

    if (config == NULL || config->device_index >= ACCEL_DEV_MAX ||
        config->sample_interval_ms == 0U ||
        config->sample_interval_ms > 1000U ||
        !accel_value_is_finite(config->low_pass_cutoff_hz))
        return false;

    sample_rate_hz = 1000U / config->sample_interval_ms;
    return config->low_pass_cutoff_hz > 0.0f &&
           config->low_pass_cutoff_hz < (float)sample_rate_hz * 0.5f;
}

static accel_service_status_t accel_service_load_calibration(
    accel_calibration_t *calibration)
{
    accel_calibration_record_t record = {0};
    storage_status_t status;
    size_t size = sizeof(record);

    if (calibration == NULL)
        return ACCEL_SERVICE_STATUS_ERROR_PARAMETER;

    *calibration = s_default_calibration;
    if (!storage_service_is_ready())
        return ACCEL_SERVICE_STATUS_NOT_READY;

    status = storage_config_load(ACCEL_CALIBRATION_STORAGE_KEY,
                                 &record, &size);
    if (status == STORAGE_STATUS_NOT_FOUND)
        return ACCEL_SERVICE_STATUS_OK;
    if (status != STORAGE_STATUS_OK && status != STORAGE_STATUS_DEGRADED)
        return ACCEL_SERVICE_STATUS_STORAGE_ERROR;
    if (size != sizeof(record) ||
        record.version != ACCEL_CALIBRATION_VERSION ||
        record.size != sizeof(record) ||
        !accel_calibration_is_valid(&record.calibration))
        return ACCEL_SERVICE_STATUS_STORAGE_ERROR;

    *calibration = record.calibration;
    return ACCEL_SERVICE_STATUS_OK;
}

static accel_service_status_t accel_service_save_calibration(
    const accel_calibration_t *calibration)
{
    const accel_calibration_record_t record = {
        .version = ACCEL_CALIBRATION_VERSION,
        .size = (uint16_t)sizeof(accel_calibration_record_t),
        .calibration = *calibration,
    };
    storage_status_t status;

    if (!storage_service_is_ready())
        return ACCEL_SERVICE_STATUS_NOT_READY;

    status = storage_config_save(ACCEL_CALIBRATION_STORAGE_KEY,
                                 &record, sizeof(record));
    return status == STORAGE_STATUS_OK || status == STORAGE_STATUS_DEGRADED
         ? ACCEL_SERVICE_STATUS_OK : ACCEL_SERVICE_STATUS_STORAGE_ERROR;
}

static bool accel_service_lock_ready(void)
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

static void accel_service_task(void *argument)
{
    accel_calibration_t calibration;
    accel_filter_config_t filter;
    osal_tick_type_t last_wake_time = 0U;
    bool initialized = false;
    bool configured = false;

    (void)argument;
    (void)accel_service_load_calibration(&calibration);
    filter.sample_rate_hz =
        (uint16_t)(1000U / s_service.config.sample_interval_ms);
    filter.low_pass_cutoff_hz = s_service.config.low_pass_cutoff_hz;
    filter.median3_enabled = s_service.config.median3_enabled;

    for (;;) {
        if (!initialized) {
            initialized = drv_adapter_accel_init(
                s_service.config.device_index);
            if (!initialized) {
                osal_task_delay_ms(s_service.config.retry_interval_ms);
                continue;
            }
        }

        if (!configured) {
            configured = drv_adapter_accel_set_filter(
                             s_service.config.device_index, &filter) &&
                         drv_adapter_accel_set_calibration(
                             s_service.config.device_index, &calibration);
            if (!configured) {
                osal_task_delay_ms(s_service.config.retry_interval_ms);
                continue;
            }
            if (osal_mutex_take(s_service.state_mutex,
                                OSAL_WAIT_FOREVER) == OSAL_SUCCESS) {
                s_service.calibration_ready = true;
                (void)osal_mutex_give(s_service.state_mutex);
            }
            last_wake_time = osal_task_get_tick_count();
        }

        if (!drv_adapter_accel_refresh(s_service.config.device_index)) {
            osal_task_delay_ms(s_service.config.retry_interval_ms);
            last_wake_time = osal_task_get_tick_count();
            continue;
        }
        osal_task_delay_until_ms(&last_wake_time,
                                 s_service.config.sample_interval_ms);
    }
}

bool accel_service_start(const accel_service_config_t *config)
{
    accel_service_config_t selected = config != NULL
                                    ? *config : s_default_config;

    if (s_service.started || !accel_service_config_is_valid(&selected))
        return false;
    if (selected.retry_interval_ms == 0U)
        selected.retry_interval_ms = ACCEL_SERVICE_DEFAULT_RETRY_INTERVAL_MS;

    s_service.config = selected;
    s_service.calibration_ready = false;
    if (osal_mutex_create(&s_service.state_mutex) != OSAL_SUCCESS)
        return false;

    s_service.started = true;
    if (osal_task_create("accel", accel_service_task,
                         ACCEL_SERVICE_TASK_STACK_SIZE_BYTES,
                         OSAL_PRIORITY_NORMAL, &s_service.task_handle,
                         NULL) != OSAL_SUCCESS) {
        s_service.started = false;
        osal_mutex_delete(s_service.state_mutex);
        s_service.state_mutex = NULL;
        return false;
    }
    return true;
}

bool accel_service_read_latest(accel_snapshot_t *snapshot)
{
    return s_service.started && snapshot != NULL &&
           drv_adapter_accel_read_snapshot(
               s_service.config.device_index, snapshot);
}

accel_service_status_t accel_service_set_calibration(
    const accel_calibration_t *calibration, bool persist)
{
    accel_service_status_t status = ACCEL_SERVICE_STATUS_OK;

    if (!accel_calibration_is_valid(calibration))
        return ACCEL_SERVICE_STATUS_ERROR_PARAMETER;
    if (!accel_service_lock_ready())
        return ACCEL_SERVICE_STATUS_NOT_READY;

    if (!drv_adapter_accel_set_calibration(
            s_service.config.device_index, calibration)) {
        status = ACCEL_SERVICE_STATUS_DRIVER_ERROR;
    } else if (persist) {
        status = accel_service_save_calibration(calibration);
    }

    if (osal_mutex_give(s_service.state_mutex) != OSAL_SUCCESS &&
        status == ACCEL_SERVICE_STATUS_OK)
        status = ACCEL_SERVICE_STATUS_DRIVER_ERROR;
    return status;
}

accel_service_status_t accel_service_get_calibration(
    accel_calibration_t *calibration)
{
    accel_service_status_t status;

    if (calibration == NULL)
        return ACCEL_SERVICE_STATUS_ERROR_PARAMETER;
    if (!accel_service_lock_ready())
        return ACCEL_SERVICE_STATUS_NOT_READY;

    status = drv_adapter_accel_get_calibration(
                 s_service.config.device_index, calibration)
           ? ACCEL_SERVICE_STATUS_OK : ACCEL_SERVICE_STATUS_DRIVER_ERROR;
    if (osal_mutex_give(s_service.state_mutex) != OSAL_SUCCESS &&
        status == ACCEL_SERVICE_STATUS_OK)
        status = ACCEL_SERVICE_STATUS_DRIVER_ERROR;
    return status;
}

accel_service_status_t accel_service_reset_calibration(bool persist)
{
    return accel_service_set_calibration(&s_default_calibration, persist);
}

bool accel_service_calculate_six_position_calibration(
    const accel_six_position_data_t *six_position,
    const accel_vector3_t *stationary_gyro_mean_dps,
    accel_calibration_t *calibration)
{
    accel_calibration_t candidate;
    float delta_x;
    float delta_y;
    float delta_z;

    if (six_position == NULL || stationary_gyro_mean_dps == NULL ||
        calibration == NULL ||
        !accel_vector_is_finite(&six_position->positive_x) ||
        !accel_vector_is_finite(&six_position->negative_x) ||
        !accel_vector_is_finite(&six_position->positive_y) ||
        !accel_vector_is_finite(&six_position->negative_y) ||
        !accel_vector_is_finite(&six_position->positive_z) ||
        !accel_vector_is_finite(&six_position->negative_z) ||
        !accel_vector_is_finite(stationary_gyro_mean_dps))
        return false;

    delta_x = six_position->positive_x.x - six_position->negative_x.x;
    delta_y = six_position->positive_y.y - six_position->negative_y.y;
    delta_z = six_position->positive_z.z - six_position->negative_z.z;
    if (accel_abs(delta_x) <= FLT_EPSILON ||
        accel_abs(delta_y) <= FLT_EPSILON ||
        accel_abs(delta_z) <= FLT_EPSILON)
        return false;

    candidate.accel_offset_g.x =
        (six_position->positive_x.x + six_position->negative_x.x) * 0.5f;
    candidate.accel_offset_g.y =
        (six_position->positive_y.y + six_position->negative_y.y) * 0.5f;
    candidate.accel_offset_g.z =
        (six_position->positive_z.z + six_position->negative_z.z) * 0.5f;
    candidate.accel_scale.x = 2.0f / delta_x;
    candidate.accel_scale.y = 2.0f / delta_y;
    candidate.accel_scale.z = 2.0f / delta_z;
    candidate.gyro_offset_dps = *stationary_gyro_mean_dps;

    if (!accel_calibration_is_valid(&candidate)) return false;
    *calibration = candidate;
    return true;
}
