#include "bsp_temp_humi_handler.h"
#include "drv_adapter_port_temphumi.h"
#include "drv_adapter_temphumi.h"
#include "iic_hal.h"
#include "osal.h"
#include "storage_service.h"
#include "temp_humi_service.h"

/* CMSIS access qualifiers collide with Win32 intrinsic parameter names. */
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

#define TEST_CONSUMER_COUNT (4U)
#define TEST_CALIBRATION_SWITCHES (100U)
#define TEST_STORAGE_CAPACITY (128U)

typedef struct {
    uint16_t version;
    uint16_t size;
    temp_humi_service_calibration_t calibration;
} test_calibration_record_t;

typedef struct {
    osal_task_entry entry;
    void *argument;
} test_task_context_t;

typedef struct {
    HANDLE thread;
    volatile LONG reads;
    volatile LONG errors;
} test_consumer_t;

typedef struct {
    const temp_humi_service_calibration_t *calibration;
    temp_humi_service_status_t status;
    HANDLE thread;
} test_calibration_writer_t;

static SRWLOCK s_storage_lock = SRWLOCK_INIT;
static uint8_t s_storage_data[TEST_STORAGE_CAPACITY];
static size_t s_storage_size;
static bool s_storage_valid;
static volatile LONG64 s_raw_sample_count;
static volatile LONG s_stop_consumers;
static volatile LONG s_force_sensor_failure;
static volatile LONG s_delay_calibration_a_save;
static volatile LONG s_slow_save_entered;

static float test_abs(float value)
{
    return value < 0.0f ? -value : value;
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
    if (milliseconds == 0U) {
        (void)SwitchToThread();
    } else {
        Sleep(milliseconds);
    }
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
    if (s_storage_valid &&
        strcmp(key, "sensor/temp_humi/calibration") == 0) {
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
        strcmp(key, "sensor/temp_humi/calibration") != 0 ||
        size > sizeof(s_storage_data))
        return STORAGE_STATUS_ERROR_PARAM;

    if (size == sizeof(*record) &&
        InterlockedCompareExchange(&s_delay_calibration_a_save, 0, 0) != 0 &&
        record->calibration.temperature_offset == 0.0f) {
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

static temp_humi_status_t test_sensor_init(bsp_temp_humi_driver_t *driver)
{
    temp_humi_iic_interface_t *iic = driver->p_iic_driver_instance;

    if (iic->pf_iic_init(iic->bus_context) != TEMP_HUMI_OK)
        return TEMP_HUMI_ERROR_RESOURCE;
    driver->is_inited = true;
    return TEMP_HUMI_OK;
}

static temp_humi_status_t test_sensor_read(bsp_temp_humi_driver_t *driver,
                                           float *temperature,
                                           float *humidity)
{
    static const float offsets[5] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    temp_humi_iic_interface_t *iic = driver->p_iic_driver_instance;
    LONG64 sample = InterlockedIncrement64(&s_raw_sample_count) - 1;
    LONG64 group = sample / 5;
    size_t point = (size_t)(sample % 5);
    float base_temperature = 20.0f + (float)(group % 10);
    temp_humi_status_t status;

    if (!driver->is_inited) return TEMP_HUMI_ERROR_RESOURCE;
    if (InterlockedCompareExchange(&s_force_sensor_failure, 0, 0) != 0)
        return TEMP_HUMI_ERROR;

    status = iic->pf_lock(iic->bus_context, 100U);
    if (status != TEMP_HUMI_OK) return status;
    Sleep(1U);
    *temperature = base_temperature + offsets[point];
    *humidity = 2.0f * (*temperature);
    return iic->pf_unlock(iic->bus_context);
}

static temp_humi_status_t test_sensor_sleep(bsp_temp_humi_driver_t *driver)
{
    driver->is_inited = false;
    return TEMP_HUMI_OK;
}

static temp_humi_status_t test_sensor_wakeup(bsp_temp_humi_driver_t *driver)
{
    driver->is_inited = true;
    return TEMP_HUMI_OK;
}

temp_humi_status_t aht21_inst(bsp_temp_humi_driver_t *driver,
                              temp_humi_iic_interface_t *iic,
                              temp_humi_yield_interface_t *yield)
{
    driver->p_iic_driver_instance = iic;
    driver->p_yield_instance = yield;
    driver->pf_init = test_sensor_init;
    driver->pf_read_temp_humi = test_sensor_read;
    driver->pf_sleep = test_sensor_sleep;
    driver->pf_wakeup = test_sensor_wakeup;
    return test_sensor_init(driver);
}

static DWORD WINAPI test_consumer_task(LPVOID argument)
{
    test_consumer_t *consumer = argument;

    while (InterlockedCompareExchange(&s_stop_consumers, 0, 0) == 0) {
        float temperature;
        float humidity;

        if (temp_humi_service_read_latest(&temperature, &humidity)) {
            float calibration_signature = humidity - 2.0f * temperature;
            if (test_abs(calibration_signature) > 0.001f &&
                test_abs(calibration_signature - 10.0f) > 0.001f)
                (void)InterlockedIncrement(&consumer->errors);
            (void)InterlockedIncrement(&consumer->reads);
        }
        (void)SwitchToThread();
    }
    return 0U;
}

static DWORD WINAPI test_calibration_writer_task(LPVOID argument)
{
    test_calibration_writer_t *writer = argument;

    writer->status = temp_humi_service_set_calibration(
        writer->calibration, true);
    return 0U;
}

static void test_store_initial_calibration(
    const temp_humi_service_calibration_t *calibration, uint16_t version)
{
    test_calibration_record_t record = {
        .version = version,
        .size = (uint16_t)sizeof(test_calibration_record_t),
        .calibration = *calibration,
    };

    assert(storage_config_save("sensor/temp_humi/calibration",
                               &record, sizeof(record)) == STORAGE_STATUS_OK);
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
    temphumi_port_config_t port_config = {
        .iic_bus = &bus,
        .bus_mutex = NULL,
        .sample_interval_ms = 0U,
        .samples_per_group = 5U,
    };
    temp_humi_service_config_t service_config = {
        .device_index = 0U,
        .sample_interval_ms = 0U,
        .retry_interval_ms = 1U,
        .samples_per_group = 5U,
    };
    const temp_humi_service_calibration_t calibration_a = {
        .temperature_scale = 1.0f,
        .temperature_offset = 0.0f,
        .humidity_scale = 1.0f,
        .humidity_offset = 0.0f,
    };
    const temp_humi_service_calibration_t calibration_b = {
        .temperature_scale = 1.0f,
        .temperature_offset = 5.0f,
        .humidity_scale = 1.0f,
        .humidity_offset = 20.0f,
    };
    temp_humi_calibration_point_t temperature_points[2] = {
        {.measured = 15.5f, .reference = 15.0f},
        {.measured = 35.8f, .reference = 35.0f},
    };
    temp_humi_calibration_point_t humidity_points[2] = {
        {.measured = 32.0f, .reference = 33.0f},
        {.measured = 77.0f, .reference = 75.0f},
    };
    temp_humi_service_calibration_t calibration;
    test_calibration_record_t saved_record;
    test_consumer_t consumers[TEST_CONSUMER_COUNT] = {0};
    test_calibration_writer_t writers[2] = {
        {.calibration = &calibration_a},
        {.calibration = &calibration_b},
    };
    float temperature;
    float humidity;
    unsigned int index;
    DWORD wait_result;
    LONG total_reads = 0;
    LONG total_errors = 0;
    const char *mode = argc > 1 ? argv[1] : "valid";
    float initial_signature;

    if (strcmp(mode, "missing") != 0)
        test_store_initial_calibration(
            &calibration_b, strcmp(mode, "corrupt") == 0 ? 99U : 1U);
    initial_signature = strcmp(mode, "valid") == 0 ? 10.0f : 0.0f;
    assert(drv_adapter_port_temphumi_register(0U, &port_config));
    assert(temp_humi_service_start(&service_config));

    for (index = 0U; index < 2000U; ++index) {
        if (temp_humi_service_read_latest(&temperature, &humidity)) break;
        Sleep(1U);
    }
    assert(index < 2000U);
    assert(test_abs(humidity - 2.0f * temperature - initial_signature) <
           0.001f);

    {
        float cached_temperature = temperature;
        float cached_humidity = humidity;
        (void)InterlockedExchange(&s_force_sensor_failure, 1);
        Sleep(20U);
        assert(temp_humi_service_read_latest(&temperature, &humidity));
        assert(temperature == cached_temperature);
        assert(humidity == cached_humidity);
        (void)InterlockedExchange(&s_force_sensor_failure, 0);
    }

    for (index = 0U; index < TEST_CONSUMER_COUNT; ++index) {
        consumers[index].thread = CreateThread(
            NULL, 0U, test_consumer_task, &consumers[index], 0U, NULL);
        assert(consumers[index].thread != NULL);
    }

    for (index = 0U; index < TEST_CALIBRATION_SWITCHES; ++index) {
        const temp_humi_service_calibration_t *selected =
            (index & 1U) == 0U ? &calibration_a : &calibration_b;
        assert(temp_humi_service_set_calibration(selected, false) ==
               TEMP_HUMI_SERVICE_STATUS_OK);
        Sleep(2U);
    }

    for (index = 0U; index < 10U; ++index) {
        assert(drv_adapter_temphumi_sleep(0U));
        assert(drv_adapter_temphumi_wakeup(0U));
        Sleep(1U);
    }

    (void)InterlockedExchange(&s_delay_calibration_a_save, 1);
    writers[0].thread = CreateThread(NULL, 0U,
                                     test_calibration_writer_task,
                                     &writers[0], 0U, NULL);
    assert(writers[0].thread != NULL);
    for (index = 0U; index < 1000U; ++index) {
        if (InterlockedCompareExchange(&s_slow_save_entered, 0, 0) != 0)
            break;
        Sleep(1U);
    }
    assert(index < 1000U);
    writers[1].thread = CreateThread(NULL, 0U,
                                     test_calibration_writer_task,
                                     &writers[1], 0U, NULL);
    assert(writers[1].thread != NULL);
    for (index = 0U; index < 2U; ++index) {
        wait_result = WaitForSingleObject(writers[index].thread, 5000U);
        assert(wait_result == WAIT_OBJECT_0);
        assert(writers[index].status == TEMP_HUMI_SERVICE_STATUS_OK);
        (void)CloseHandle(writers[index].thread);
    }
    (void)InterlockedExchange(&s_delay_calibration_a_save, 0);
    Sleep(500U);
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

    assert(temp_humi_service_get_calibration(&calibration) ==
           TEMP_HUMI_SERVICE_STATUS_OK);
    assert(memcmp(&calibration, &calibration_b, sizeof(calibration)) == 0);

    AcquireSRWLockShared(&s_storage_lock);
    assert(s_storage_size == sizeof(saved_record));
    memcpy(&saved_record, s_storage_data, sizeof(saved_record));
    ReleaseSRWLockShared(&s_storage_lock);
    assert(saved_record.version == 1U);
    assert(saved_record.size == sizeof(saved_record));
    assert(memcmp(&saved_record.calibration, &calibration_b,
                  sizeof(calibration_b)) == 0);

    calibration = calibration_b;
    calibration.temperature_scale = 0.1f;
    assert(temp_humi_service_set_calibration(&calibration, true) ==
           TEMP_HUMI_SERVICE_STATUS_ERROR_PARAMETER);

    assert(temp_humi_service_calculate_two_point_calibration(
        temperature_points, humidity_points, &calibration));
    assert(test_abs(calibration.temperature_scale * 15.5f +
                    calibration.temperature_offset - 15.0f) < 0.0001f);
    assert(test_abs(calibration.humidity_scale * 77.0f +
                    calibration.humidity_offset - 75.0f) < 0.0001f);

    printf("PASS: mode=%s reads=%ld switches=%u writers=2 raw_samples=%lld\n",
           mode, (long)total_reads, TEST_CALIBRATION_SWITCHES,
           (long long)InterlockedCompareExchange64(
               &s_raw_sample_count, 0, 0));
    return 0;
}
