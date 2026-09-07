#include "accel_service.h"
#include "bsp_accel_handler.h"
#include "drv_adapter_port_accel.h"
#include "drv_adapter_accel.h"
#include "iic_hal.h"
#include "osal.h"
#include "storage_service.h"

#undef __I
#undef __O
#undef __IO
#undef __IM
#undef __OM
#undef __IOM
#undef CRC

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_CONSUMER_COUNT       (4U)
#define TEST_CALIBRATION_SWITCHES (100U)
#define TEST_STORAGE_CAPACITY     (128U)

typedef struct
{
    uint16_t version;
    uint16_t size;
    accel_calibration_t calibration;
} test_calibration_record_t;

typedef struct
{
    osal_task_entry entry;
    void *argument;
} test_task_context_t;

typedef struct
{
    HANDLE thread;
    volatile LONG reads;
    volatile LONG errors;
    uint32_t last_sequence;
} test_consumer_t;

typedef struct
{
    const accel_calibration_t *calibration;
    accel_service_status_t status;
    HANDLE thread;
} test_writer_t;

static SRWLOCK s_storage_lock = SRWLOCK_INIT;
static uint8_t s_storage_data[TEST_STORAGE_CAPACITY];
static size_t s_storage_size;
static bool s_storage_valid;
static volatile LONG64 s_raw_sample_count;
static volatile LONG s_stop_consumers;
static volatile LONG s_force_sensor_failure;
static volatile LONG s_filter_test_mode;
static volatile LONG s_filter_test_index;
static volatile LONG s_delay_identity_save;
static volatile LONG s_slow_save_entered;

static const accel_calibration_t s_calibration_a = {
    .accel_offset_g = {0.0f, 0.0f, 0.0f},
    .accel_scale = {1.0f, 1.0f, 1.0f},
    .gyro_offset_dps = {0.0f, 0.0f, 0.0f},
};

static const accel_calibration_t s_calibration_b = {
    .accel_offset_g = {0.1f, -0.2f, 0.05f},
    .accel_scale = {1.1f, 0.9f, 1.0f},
    .gyro_offset_dps = {2.0f, -3.0f, 4.0f},
};

static float test_abs(float value)
{
    return value < 0.0f ? -value : value;
}

static bool test_near(float first, float second)
{
    return test_abs(first - second) < 0.001f;
}

static DWORD WINAPI test_task_trampoline(LPVOID argument)
{
    test_task_context_t *context = argument;
    osal_task_entry entry = context->entry;
    void *entry_argument = context->argument;

    free(context);
    entry(entry_argument);
    return 0U;
}

int32_t osal_task_create(const char *task_name, osal_task_entry entry,
                         size_t stack_size, osal_priority_t priority,
                         osal_task_handle_t *task_handle, void *argument)
{
    test_task_context_t *context;
    HANDLE thread;

    (void)task_name;
    (void)priority;
    if (entry == NULL || task_handle == NULL) return OSAL_INVALID_POINTER;
    context = malloc(sizeof(*context));
    if (context == NULL) return OSAL_ERROR;
    context->entry = entry;
    context->argument = argument;
    thread = CreateThread(NULL, stack_size, test_task_trampoline,
                          context, 0U, NULL);
    if (thread == NULL) {
        free(context);
        return OSAL_ERROR;
    }
    *task_handle = thread;
    return OSAL_SUCCESS;
}

void osal_task_delay_ms(uint32_t milliseconds)
{
    if (milliseconds == 0U) (void)SwitchToThread();
    else Sleep(milliseconds);
}

osal_tick_type_t osal_task_get_tick_count(void)
{
    return (osal_tick_type_t)GetTickCount();
}

osal_time_ms_t osal_time_get_ms(void)
{
    return (osal_time_ms_t)GetTickCount();
}

void osal_task_delay_until_ms(osal_tick_type_t *last_wake_time,
                              uint32_t period_ms)
{
    uint32_t target;
    uint32_t now;

    if (last_wake_time == NULL) return;
    target = *last_wake_time + period_ms;
    now = GetTickCount();
    if ((int32_t)(target - now) > 0) Sleep(target - now);
    *last_wake_time = target;
}

int32_t osal_mutex_create(osal_mutex_handle_t *mutex_handle)
{
    HANDLE mutex;

    if (mutex_handle == NULL) return OSAL_INVALID_POINTER;
    mutex = CreateMutexA(NULL, FALSE, NULL);
    if (mutex == NULL) return OSAL_ERROR;
    *mutex_handle = mutex;
    return OSAL_SUCCESS;
}

void osal_mutex_delete(osal_mutex_handle_t mutex_handle)
{
    if (mutex_handle != NULL) (void)CloseHandle(mutex_handle);
}

int32_t osal_mutex_take(osal_mutex_handle_t mutex_handle,
                        osal_time_ms_t timeout_ms)
{
    DWORD timeout = timeout_ms == OSAL_WAIT_FOREVER
                  ? INFINITE : (DWORD)timeout_ms;
    DWORD result;

    if (mutex_handle == NULL) return OSAL_INVALID_POINTER;
    result = WaitForSingleObject(mutex_handle, timeout);
    return result == WAIT_OBJECT_0 ? OSAL_SUCCESS : OSAL_ERROR_TIMEOUT;
}

int32_t osal_mutex_give(osal_mutex_handle_t mutex_handle)
{
    if (mutex_handle == NULL) return OSAL_INVALID_POINTER;
    return ReleaseMutex(mutex_handle) ? OSAL_SUCCESS : OSAL_ERROR;
}

bool storage_service_is_ready(void)
{
    return true;
}

storage_status_t storage_config_load(const char *key, void *data, size_t *size)
{
    storage_status_t status = STORAGE_STATUS_NOT_FOUND;

    if (key == NULL || data == NULL || size == NULL)
        return STORAGE_STATUS_ERROR_PARAM;
    AcquireSRWLockShared(&s_storage_lock);
    if (s_storage_valid && strcmp(key, "sensor/accel/calibration") == 0) {
        if (*size < s_storage_size) {
            status = STORAGE_STATUS_NO_SPACE;
        } else {
            memcpy(data, s_storage_data, s_storage_size);
            *size = s_storage_size;
            status = STORAGE_STATUS_OK;
        }
    }
    ReleaseSRWLockShared(&s_storage_lock);
    return status;
}

storage_status_t storage_config_save(const char *key, const void *data,
                                     size_t size)
{
    const test_calibration_record_t *record = data;

    if (key == NULL || data == NULL ||
        strcmp(key, "sensor/accel/calibration") != 0 ||
        size > sizeof(s_storage_data))
        return STORAGE_STATUS_ERROR_PARAM;
    if (size == sizeof(*record) &&
        InterlockedCompareExchange(&s_delay_identity_save, 0, 0) != 0 &&
        record->calibration.accel_offset_g.x == 0.0f) {
        (void)InterlockedExchange(&s_slow_save_entered, 1);
        Sleep(30U);
    }
    AcquireSRWLockExclusive(&s_storage_lock);
    memcpy(s_storage_data, data, size);
    s_storage_size = size;
    s_storage_valid = true;
    ReleaseSRWLockExclusive(&s_storage_lock);
    return STORAGE_STATUS_OK;
}

void IICStart(iic_bus_t *bus) { (void)bus; }
void IICStop(iic_bus_t *bus) { (void)bus; }
unsigned char IICWaitAck(iic_bus_t *bus) { (void)bus; return SUCCESS; }
void IICSendAck(iic_bus_t *bus) { (void)bus; }
void IICSendNotAck(iic_bus_t *bus) { (void)bus; }
void IICSendByte(iic_bus_t *bus, unsigned char data)
{
    (void)bus;
    (void)data;
}
unsigned char IICReceiveByte(iic_bus_t *bus) { (void)bus; return 0U; }
void IICInit(iic_bus_t *bus) { (void)bus; }

static accel_status_t test_sensor_init(bsp_accel_driver_t *driver)
{
    if (driver == NULL || driver->p_iic_driver_instance == NULL)
        return ACCEL_ERROR_PARAMETER;
    if (driver->p_iic_driver_instance->pf_iic_init(
            driver->p_iic_driver_instance->bus_context) != ACCEL_OK)
        return ACCEL_ERROR_RESOURCE;
    driver->is_inited = true;
    return ACCEL_OK;
}

static accel_status_t test_sensor_read_imu(bsp_accel_driver_t *driver,
                                           accel_imu_data_t *imu)
{
    accel_iic_interface_t *iic = driver->p_iic_driver_instance;
    accel_status_t status;

    if (!driver->is_inited || imu == NULL) return ACCEL_ERROR_RESOURCE;
    if (InterlockedCompareExchange(&s_force_sensor_failure, 0, 0) != 0)
        return ACCEL_ERROR;
    status = iic->pf_lock != NULL
           ? iic->pf_lock(iic->bus_context, 100U) : ACCEL_OK;
    if (status != ACCEL_OK) return status;
    Sleep(1U);

    if (InterlockedCompareExchange(&s_filter_test_mode, 0, 0) != 0) {
        static const float samples[3] = {1.0f, 1.0f, 100.0f};
        LONG index = InterlockedIncrement(&s_filter_test_index) - 1;
        float value = samples[index < 3 ? index : 2];
        imu->accel_g = (accel_data_t){value, value, value};
        imu->gyro_dps = (accel_data_t){value, value, value};
    } else {
        imu->accel_g = (accel_data_t){0.1f, -0.2f, 1.05f};
        imu->gyro_dps = (accel_data_t){2.0f, -3.0f, 4.0f};
    }
    imu->temperature_c = 30.0f;
    (void)InterlockedIncrement64(&s_raw_sample_count);
    return iic->pf_unlock != NULL
         ? iic->pf_unlock(iic->bus_context) : ACCEL_OK;
}

static accel_status_t test_sensor_sleep(bsp_accel_driver_t *driver)
{
    driver->is_inited = false;
    return ACCEL_OK;
}

static accel_status_t test_sensor_wakeup(bsp_accel_driver_t *driver)
{
    driver->is_inited = true;
    return ACCEL_OK;
}

accel_status_t mpu6050_inst(bsp_accel_driver_t *driver,
                            accel_iic_interface_t *iic,
                            accel_yield_interface_t *yield)
{
    if (driver == NULL || iic == NULL || yield == NULL)
        return ACCEL_ERROR_PARAMETER;
    driver->p_iic_driver_instance = iic;
    driver->p_yield_instance = yield;
    driver->pf_init = test_sensor_init;
    driver->pf_read_imu = test_sensor_read_imu;
    driver->pf_sleep = test_sensor_sleep;
    driver->pf_wakeup = test_sensor_wakeup;
    return test_sensor_init(driver);
}

static bool test_snapshot_matches_a(const accel_snapshot_t *snapshot)
{
    return test_near(snapshot->accel_g.x, 0.1f) &&
           test_near(snapshot->accel_g.y, -0.2f) &&
           test_near(snapshot->accel_g.z, 1.05f) &&
           test_near(snapshot->gyro_dps.x, 2.0f) &&
           test_near(snapshot->gyro_dps.y, -3.0f) &&
           test_near(snapshot->gyro_dps.z, 4.0f) &&
           test_near(snapshot->temperature_c, 30.0f);
}

static bool test_snapshot_matches_b(const accel_snapshot_t *snapshot)
{
    return test_near(snapshot->accel_g.x, 0.0f) &&
           test_near(snapshot->accel_g.y, 0.0f) &&
           test_near(snapshot->accel_g.z, 1.0f) &&
           test_near(snapshot->gyro_dps.x, 0.0f) &&
           test_near(snapshot->gyro_dps.y, 0.0f) &&
           test_near(snapshot->gyro_dps.z, 0.0f) &&
           test_near(snapshot->temperature_c, 30.0f);
}

static DWORD WINAPI test_consumer_task(LPVOID argument)
{
    test_consumer_t *consumer = argument;

    while (InterlockedCompareExchange(&s_stop_consumers, 0, 0) == 0) {
        accel_snapshot_t snapshot;

        if (accel_service_read_latest(&snapshot)) {
            if ((!test_snapshot_matches_a(&snapshot) &&
                 !test_snapshot_matches_b(&snapshot)) ||
                snapshot.sequence == 0U ||
                snapshot.sequence < consumer->last_sequence)
                (void)InterlockedIncrement(&consumer->errors);
            consumer->last_sequence = snapshot.sequence;
            (void)InterlockedIncrement(&consumer->reads);
        }
        (void)SwitchToThread();
    }
    return 0U;
}

static DWORD WINAPI test_writer_task(LPVOID argument)
{
    test_writer_t *writer = argument;
    writer->status = accel_service_set_calibration(writer->calibration, true);
    return 0U;
}

static void test_store_initial_calibration(
    const accel_calibration_t *calibration, uint16_t version)
{
    const test_calibration_record_t record = {
        .version = version,
        .size = (uint16_t)sizeof(test_calibration_record_t),
        .calibration = *calibration,
    };
    assert(storage_config_save("sensor/accel/calibration",
                               &record, sizeof(record)) == STORAGE_STATUS_OK);
}

static void test_handler_filter(void)
{
    bsp_accel_handler_t handler;
    accel_iic_interface_t iic = {0};
    bsp_accel_filter_config_t filter = {
        .sample_rate_hz = 100U,
        .low_pass_cutoff_hz = 15.0f,
        .median3_enabled = true,
    };
    accel_imu_data_t imu;

    (void)InterlockedExchange(&s_filter_test_mode, 1);
    (void)InterlockedExchange(&s_filter_test_index, 0);
    memset(&handler, 0, sizeof(handler));
    handler.calibration.accel_scale = (accel_data_t){1.0f, 1.0f, 1.0f};
    handler.filter_config = filter;
    handler.initialized = true;
    handler.driver.is_inited = true;
    handler.driver.p_iic_driver_instance = &iic;
    handler.driver.pf_read_imu = test_sensor_read_imu;
    assert(accel_handler_read_imu(&handler, &imu) == ACCEL_OK);
    assert(accel_handler_read_imu(&handler, &imu) == ACCEL_OK);
    assert(accel_handler_read_imu(&handler, &imu) == ACCEL_OK);
    assert(test_near(imu.accel_g.x, 1.0f));
    assert(test_near(imu.gyro_dps.z, 1.0f));

    filter.median3_enabled = false;
    assert(accel_handler_set_filter(&handler, &filter) == ACCEL_OK);
    (void)InterlockedExchange(&s_filter_test_index, 0);
    assert(accel_handler_read_imu(&handler, &imu) == ACCEL_OK);
    assert(accel_handler_read_imu(&handler, &imu) == ACCEL_OK);
    assert(accel_handler_read_imu(&handler, &imu) == ACCEL_OK);
    assert(imu.accel_g.x > 1.0f && imu.accel_g.x < 100.0f);
    (void)InterlockedExchange(&s_filter_test_mode, 0);
}

int main(int argc, char **argv)
{
    GPIO_TypeDef fake_sda_port = {0};
    GPIO_TypeDef fake_scl_port = {0};
    iic_bus_t bus = {
        .IIC_SDA_PORT = &fake_sda_port,
        .IIC_SCL_PORT = &fake_scl_port,
        .IIC_SDA_PIN = GPIO_PIN_0,
        .IIC_SCL_PIN = GPIO_PIN_1,
    };
    accel_port_config_t port_config = {
        .iic_bus = &bus,
        .bus_mutex = NULL,
    };
    accel_service_config_t service_config = {
        .device_index = 0U,
        .sample_interval_ms = 5U,
        .retry_interval_ms = 1U,
        .low_pass_cutoff_hz = 15.0f,
        .median3_enabled = true,
    };
    accel_six_position_data_t six_position = {
        .positive_x = {1.02f, 0.0f, 0.0f},
        .negative_x = {-0.98f, 0.0f, 0.0f},
        .positive_y = {0.0f, 1.10f, 0.0f},
        .negative_y = {0.0f, -0.90f, 0.0f},
        .positive_z = {0.0f, 0.0f, 0.95f},
        .negative_z = {0.0f, 0.0f, -1.05f},
    };
    accel_vector3_t gyro_mean = {1.0f, -2.0f, 3.0f};
    accel_calibration_t calibration;
    accel_snapshot_t snapshot_before;
    accel_snapshot_t snapshot_after;
    test_calibration_record_t saved_record;
    test_consumer_t consumers[TEST_CONSUMER_COUNT] = {0};
    test_writer_t writers[2] = {
        {.calibration = &s_calibration_a},
        {.calibration = &s_calibration_b},
    };
    const char *mode = argc > 1 ? argv[1] : "valid";
    bool initial_is_b = strcmp(mode, "valid") == 0;
    LONG total_reads = 0;
    LONG total_errors = 0;
    DWORD wait_result;
    unsigned int index;

    test_handler_filter();
    if (strcmp(mode, "missing") != 0)
        test_store_initial_calibration(
            &s_calibration_b, strcmp(mode, "corrupt") == 0 ? 99U : 1U);
    assert(drv_adapter_port_accel_register(0U, &port_config));
    assert(accel_service_start(&service_config));
    for (index = 0U; index < 2000U; ++index) {
        if (accel_service_read_latest(&snapshot_after)) break;
        Sleep(1U);
    }
    assert(index < 2000U);
    assert(initial_is_b ? test_snapshot_matches_b(&snapshot_after)
                        : test_snapshot_matches_a(&snapshot_after));

    (void)InterlockedExchange(&s_force_sensor_failure, 1);
    Sleep(20U);
    assert(accel_service_read_latest(&snapshot_before));
    Sleep(20U);
    assert(accel_service_read_latest(&snapshot_after));
    assert(memcmp(&snapshot_before, &snapshot_after,
                  sizeof(snapshot_before)) == 0);
    (void)InterlockedExchange(&s_force_sensor_failure, 0);

    for (index = 0U; index < TEST_CONSUMER_COUNT; ++index) {
        consumers[index].thread = CreateThread(
            NULL, 0U, test_consumer_task, &consumers[index], 0U, NULL);
        assert(consumers[index].thread != NULL);
    }
    for (index = 0U; index < TEST_CALIBRATION_SWITCHES; ++index) {
        const accel_calibration_t *selected = (index & 1U) == 0U
                                            ? &s_calibration_a
                                            : &s_calibration_b;
        assert(accel_service_set_calibration(selected, false) ==
               ACCEL_SERVICE_STATUS_OK);
        Sleep(2U);
    }
    for (index = 0U; index < 10U; ++index) {
        assert(drv_adapter_accel_sleep(0U));
        assert(drv_adapter_accel_wakeup(0U));
        Sleep(1U);
    }

    (void)InterlockedExchange(&s_delay_identity_save, 1);
    writers[0].thread = CreateThread(NULL, 0U, test_writer_task,
                                     &writers[0], 0U, NULL);
    assert(writers[0].thread != NULL);
    for (index = 0U; index < 1000U; ++index) {
        if (InterlockedCompareExchange(&s_slow_save_entered, 0, 0) != 0)
            break;
        Sleep(1U);
    }
    assert(index < 1000U);
    writers[1].thread = CreateThread(NULL, 0U, test_writer_task,
                                     &writers[1], 0U, NULL);
    assert(writers[1].thread != NULL);
    for (index = 0U; index < 2U; ++index) {
        wait_result = WaitForSingleObject(writers[index].thread, 5000U);
        assert(wait_result == WAIT_OBJECT_0);
        assert(writers[index].status == ACCEL_SERVICE_STATUS_OK);
        (void)CloseHandle(writers[index].thread);
    }
    (void)InterlockedExchange(&s_delay_identity_save, 0);
    Sleep(300U);
    (void)InterlockedExchange(&s_stop_consumers, 1);
    for (index = 0U; index < TEST_CONSUMER_COUNT; ++index) {
        wait_result = WaitForSingleObject(consumers[index].thread, 5000U);
        assert(wait_result == WAIT_OBJECT_0);
        total_reads += consumers[index].reads;
        total_errors += consumers[index].errors;
        (void)CloseHandle(consumers[index].thread);
    }
    assert(total_reads > 1000);
    assert(total_errors == 0);

    assert(accel_service_get_calibration(&calibration) ==
           ACCEL_SERVICE_STATUS_OK);
    assert(memcmp(&calibration, &s_calibration_b, sizeof(calibration)) == 0);
    AcquireSRWLockShared(&s_storage_lock);
    assert(s_storage_size == sizeof(saved_record));
    memcpy(&saved_record, s_storage_data, sizeof(saved_record));
    ReleaseSRWLockShared(&s_storage_lock);
    assert(saved_record.version == 1U);
    assert(memcmp(&saved_record.calibration, &s_calibration_b,
                  sizeof(s_calibration_b)) == 0);

    calibration = s_calibration_b;
    calibration.accel_scale.x = 0.1f;
    assert(accel_service_set_calibration(&calibration, true) ==
           ACCEL_SERVICE_STATUS_ERROR_PARAMETER);
    assert(accel_service_calculate_six_position_calibration(
        &six_position, &gyro_mean, &calibration));
    assert(test_near(calibration.accel_offset_g.x, 0.02f));
    assert(test_near(calibration.accel_offset_g.y, 0.10f));
    assert(test_near(calibration.accel_offset_g.z, -0.05f));
    assert(test_near(calibration.accel_scale.x, 1.0f));
    assert(test_near(calibration.gyro_offset_dps.y, -2.0f));

    printf("PASS: mode=%s reads=%ld switches=%u writers=2 raw_samples=%lld\n",
           mode, (long)total_reads, TEST_CALIBRATION_SWITCHES,
           (long long)InterlockedCompareExchange64(
               &s_raw_sample_count, 0, 0));
    return 0;
}
