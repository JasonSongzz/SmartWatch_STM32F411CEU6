#include "bsp_spiflash_driver.h"
#include "drv_adapter_flash.h"
#include "drv_adapter_port_flash.h"
#include "osal.h"
#include "spi.h"
#include "spiflash_layout.h"
#include "storage_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define CONFIG_WRITER_COUNT      4U
#define CONFIG_WRITES_PER_THREAD 150U
#define CONFIG_READ_COUNT        1000U
#define LOG_WRITER_COUNT         4U
#define LOGS_PER_THREAD          100U

typedef struct
{
    CRITICAL_SECTION native;
} test_mutex_t;

typedef struct
{
    uint32_t writer;
    uint32_t sequence;
    uint32_t inverse;
    uint8_t fill[52];
} config_payload_t;

typedef struct
{
    uint32_t writer;
    uint32_t sequence;
    uint32_t checksum;
} log_payload_t;

static uint8_t *s_flash_memory;
static CRITICAL_SECTION s_flash_mutex;
static volatile LONG s_config_failures;
static volatile LONG s_log_failures;
static volatile LONG s_log_visited;
static HANDLE s_slow_visitor_entered;

SPI_HandleTypeDef hspi2;

int32_t osal_mutex_create(osal_mutex_handle_t *mutex_handle)
{
    test_mutex_t *mutex;
    if (mutex_handle == NULL) return OSAL_INVALID_POINTER;
    mutex = malloc(sizeof(*mutex));
    if (mutex == NULL) return OSAL_ERROR;
    InitializeCriticalSection(&mutex->native);
    *mutex_handle = mutex;
    return OSAL_SUCCESS;
}

void osal_mutex_delete(osal_mutex_handle_t mutex_handle)
{
    test_mutex_t *mutex = mutex_handle;
    if (mutex != NULL)
    {
        DeleteCriticalSection(&mutex->native);
        free(mutex);
    }
}

int32_t osal_mutex_take(osal_mutex_handle_t mutex_handle,
                        osal_time_ms_t timeout_ms)
{
    test_mutex_t *mutex = mutex_handle;
    (void)timeout_ms;
    if (mutex == NULL) return OSAL_INVALID_POINTER;
    EnterCriticalSection(&mutex->native);
    return OSAL_SUCCESS;
}

int32_t osal_mutex_give(osal_mutex_handle_t mutex_handle)
{
    test_mutex_t *mutex = mutex_handle;
    if (mutex == NULL) return OSAL_INVALID_POINTER;
    LeaveCriticalSection(&mutex->native);
    return OSAL_SUCCESS;
}

void osal_task_delay_ms(uint32_t milliseconds)
{
    Sleep(milliseconds);
}

void *osal_heap_malloc(size_t wanted_size) { return malloc(wanted_size); }
void osal_heap_free(void *pointer) { free(pointer); }

void HAL_GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *init)
{
    (void)gpio;
    (void)init;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *gpio, uint16_t pin, GPIO_PinState state)
{
    (void)gpio;
    (void)pin;
    (void)state;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *spi, const uint8_t *data,
                                   uint16_t size, uint32_t timeout)
{
    (void)spi;
    (void)data;
    (void)size;
    (void)timeout;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *spi, uint8_t *data,
                                  uint16_t size, uint32_t timeout)
{
    (void)spi;
    (void)timeout;
    memset(data, 0xFF, size);
    return HAL_OK;
}

spiflash_status_t spiflash_inst(
    bsp_spiflash_driver_t *driver,
    const spiflash_spi_driver_interface_t *spi,
    const spiflash_yield_interface_t *yield)
{
    if (driver == NULL || spi == NULL || yield == NULL)
        return SPIFLASH_ERROR_PARAMETER;
    driver->device = driver;
    driver->spi = *spi;
    driver->yield = *yield;
    return SPIFLASH_OK;
}

bool spiflash_is_ready(const bsp_spiflash_driver_t *driver)
{
    return driver != NULL && driver->device != NULL;
}

spiflash_status_t spiflash_get_info(bsp_spiflash_driver_t *driver,
                                    spiflash_info_t *info)
{
    if (!spiflash_is_ready(driver)) return SPIFLASH_ERROR_NOT_READY;
    if (info == NULL) return SPIFLASH_ERROR_PARAMETER;
    info->capacity = SPIFLASH_TOTAL_SIZE;
    info->erase_size = SPIFLASH_SECTOR_SIZE;
    info->write_granularity_bits = 1U;
    return SPIFLASH_OK;
}

spiflash_status_t spiflash_read(bsp_spiflash_driver_t *driver,
                                uint32_t address, void *buffer, size_t size)
{
    if (!spiflash_is_ready(driver)) return SPIFLASH_ERROR_NOT_READY;
    if ((size > 0U && buffer == NULL) || address > SPIFLASH_TOTAL_SIZE ||
        size > SPIFLASH_TOTAL_SIZE - address)
        return SPIFLASH_ERROR_PARAMETER;
    EnterCriticalSection(&s_flash_mutex);
    memcpy(buffer, &s_flash_memory[address], size);
    LeaveCriticalSection(&s_flash_mutex);
    return SPIFLASH_OK;
}

spiflash_status_t spiflash_write(bsp_spiflash_driver_t *driver,
                                 uint32_t address, const void *buffer,
                                 size_t size)
{
    size_t index;
    const uint8_t *source = buffer;
    if (!spiflash_is_ready(driver)) return SPIFLASH_ERROR_NOT_READY;
    if ((size > 0U && buffer == NULL) || address > SPIFLASH_TOTAL_SIZE ||
        size > SPIFLASH_TOTAL_SIZE - address)
        return SPIFLASH_ERROR_PARAMETER;
    EnterCriticalSection(&s_flash_mutex);
    for (index = 0U; index < size; ++index)
        s_flash_memory[address + index] &= source[index];
    LeaveCriticalSection(&s_flash_mutex);
    return SPIFLASH_OK;
}

spiflash_status_t spiflash_erase(bsp_spiflash_driver_t *driver,
                                 uint32_t address, size_t size)
{
    if (!spiflash_is_ready(driver)) return SPIFLASH_ERROR_NOT_READY;
    if (address > SPIFLASH_TOTAL_SIZE || size > SPIFLASH_TOTAL_SIZE - address ||
        (address % SPIFLASH_SECTOR_SIZE) != 0U ||
        (size % SPIFLASH_SECTOR_SIZE) != 0U)
        return SPIFLASH_ERROR_PARAMETER;
    EnterCriticalSection(&s_flash_mutex);
    memset(&s_flash_memory[address], 0xFF, size);
    LeaveCriticalSection(&s_flash_mutex);
    return SPIFLASH_OK;
}

static bool config_valid(const config_payload_t *payload)
{
    size_t index;
    uint8_t expected = (uint8_t)(payload->writer ^ payload->sequence);
    if (payload->inverse != ~payload->sequence) return false;
    for (index = 0U; index < sizeof(payload->fill); ++index)
        if (payload->fill[index] != expected) return false;
    return true;
}

static DWORD WINAPI config_writer(LPVOID parameter)
{
    uint32_t writer = (uint32_t)(uintptr_t)parameter;
    uint32_t sequence;
    config_payload_t payload;

    for (sequence = 0U; sequence < CONFIG_WRITES_PER_THREAD; ++sequence)
    {
        payload.writer = writer;
        payload.sequence = sequence;
        payload.inverse = ~sequence;
        memset(payload.fill, (uint8_t)(writer ^ sequence),
               sizeof(payload.fill));
        if (storage_config_save("stress_cfg", &payload, sizeof(payload)) !=
            STORAGE_STATUS_OK)
            InterlockedIncrement(&s_config_failures);
        SwitchToThread();
    }
    return 0U;
}

static DWORD WINAPI config_reader(LPVOID parameter)
{
    uint32_t index;
    (void)parameter;
    for (index = 0U; index < CONFIG_READ_COUNT; ++index)
    {
        config_payload_t payload;
        size_t size = sizeof(payload);
        storage_status_t status = storage_config_load(
            "stress_cfg", &payload, &size);
        if (status != STORAGE_STATUS_OK || size != sizeof(payload) ||
            !config_valid(&payload))
            InterlockedIncrement(&s_config_failures);
        SwitchToThread();
    }
    return 0U;
}

static DWORD WINAPI log_writer(LPVOID parameter)
{
    uint32_t writer = (uint32_t)(uintptr_t)parameter;
    uint32_t sequence;
    for (sequence = 0U; sequence < LOGS_PER_THREAD; ++sequence)
    {
        log_payload_t payload = {
            .writer = writer,
            .sequence = sequence,
            .checksum = writer ^ sequence ^ 0xA55AA55AU,
        };
        if (storage_log_append((storage_log_time_t)sequence,
                               &payload, sizeof(payload)) != STORAGE_STATUS_OK)
            InterlockedIncrement(&s_log_failures);
        SwitchToThread();
    }
    return 0U;
}

static bool validate_log(flash_log_time_t timestamp, const void *data,
                         size_t size, void *argument)
{
    const log_payload_t *payload = data;
    (void)timestamp;
    (void)argument;
    if (size != sizeof(*payload) ||
        payload->checksum !=
            (payload->writer ^ payload->sequence ^ 0xA55AA55AU))
        InterlockedIncrement(&s_log_failures);
    InterlockedIncrement(&s_log_visited);
    return false;
}

static bool slow_log_visitor(flash_log_time_t timestamp, const void *data,
                             size_t size, void *argument)
{
    log_payload_t reentrant = {98U, 1U, 98U ^ 1U ^ 0xA55AA55AU};
    (void)timestamp;
    (void)data;
    (void)size;
    (void)argument;
    assert(storage_log_append(0, &reentrant, sizeof(reentrant)) ==
           STORAGE_STATUS_OK);
    SetEvent(s_slow_visitor_entered);
    Sleep(120U);
    return true;
}

static DWORD WINAPI slow_visit_thread(LPVOID parameter)
{
    (void)parameter;
    assert(storage_log_visit_latest(1U, slow_log_visitor, NULL) == 1U);
    return 0U;
}

static void test_raw_regions(void)
{
    uint8_t write_data[64];
    uint8_t read_data[64];
    size_t index;

    for (index = 0U; index < sizeof(write_data); ++index)
        write_data[index] = (uint8_t)index;
    assert(storage_image_erase(0U, SPIFLASH_SECTOR_SIZE) == STORAGE_STATUS_OK);
    assert(storage_image_write(32U, write_data, sizeof(write_data)) ==
           STORAGE_STATUS_OK);
    assert(storage_image_read(32U, read_data, sizeof(read_data)) ==
           STORAGE_STATUS_OK);
    assert(memcmp(write_data, read_data, sizeof(write_data)) == 0);
    assert(storage_image_erase(1U, SPIFLASH_SECTOR_SIZE) ==
           STORAGE_STATUS_ALIGNMENT_ERROR);
    assert(storage_image_read(SPIFLASH_IMAGE_SIZE - 4U, read_data, 8U) ==
           STORAGE_STATUS_OUT_OF_RANGE);
}

int main(void)
{
    HANDLE config_threads[CONFIG_WRITER_COUNT + 1U];
    HANDLE log_threads[LOG_WRITER_COUNT];
    config_payload_t initial = {0U, 0U, UINT32_MAX, {0}};
    log_payload_t latency_record = {99U, 1U, 99U ^ 1U ^ 0xA55AA55AU};
    HANDLE visit_thread;
    ULONGLONG start_ms;
    ULONGLONG append_latency_ms;
    uint32_t index;

    InitializeCriticalSection(&s_flash_mutex);
    s_flash_memory = malloc(SPIFLASH_TOTAL_SIZE);
    assert(s_flash_memory != NULL);
    memset(s_flash_memory, 0xFF, SPIFLASH_TOTAL_SIZE);

    assert(drv_adapter_port_flash_register(FLASH_DEV_EXTERNAL));
    assert(storage_service_init() == STORAGE_STATUS_OK);
    assert(storage_service_is_ready());
    test_raw_regions();

    assert(storage_config_save("stress_cfg", &initial, sizeof(initial)) ==
           STORAGE_STATUS_OK);
    for (index = 0U; index < CONFIG_WRITER_COUNT; ++index)
        config_threads[index] = CreateThread(
            NULL, 0, config_writer, (LPVOID)(uintptr_t)(index + 1U), 0, NULL);
    config_threads[CONFIG_WRITER_COUNT] =
        CreateThread(NULL, 0, config_reader, NULL, 0, NULL);
    assert(WaitForMultipleObjects(CONFIG_WRITER_COUNT + 1U, config_threads,
                                  TRUE, 60000U) == WAIT_OBJECT_0);
    for (index = 0U; index < CONFIG_WRITER_COUNT + 1U; ++index)
        CloseHandle(config_threads[index]);
    assert(s_config_failures == 0);

    assert(storage_log_clear() == STORAGE_STATUS_OK);
    for (index = 0U; index < LOG_WRITER_COUNT; ++index)
        log_threads[index] = CreateThread(
            NULL, 0, log_writer, (LPVOID)(uintptr_t)(index + 1U), 0, NULL);
    assert(WaitForMultipleObjects(LOG_WRITER_COUNT, log_threads, TRUE,
                                  60000U) == WAIT_OBJECT_0);
    for (index = 0U; index < LOG_WRITER_COUNT; ++index)
        CloseHandle(log_threads[index]);
    assert(s_log_failures == 0);
    assert(storage_log_visit_latest(LOG_WRITER_COUNT * LOGS_PER_THREAD,
                                    validate_log, NULL) ==
           LOG_WRITER_COUNT * LOGS_PER_THREAD);
    assert(s_log_visited == (LONG)(LOG_WRITER_COUNT * LOGS_PER_THREAD));

    s_slow_visitor_entered = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(s_slow_visitor_entered != NULL);
    visit_thread = CreateThread(NULL, 0, slow_visit_thread, NULL, 0, NULL);
    assert(visit_thread != NULL);
    assert(WaitForSingleObject(s_slow_visitor_entered, 5000U) == WAIT_OBJECT_0);
    start_ms = GetTickCount64();
    assert(storage_log_append(0, &latency_record, sizeof(latency_record)) ==
           STORAGE_STATUS_OK);
    append_latency_ms = GetTickCount64() - start_ms;
    assert(WaitForSingleObject(visit_thread, 5000U) == WAIT_OBJECT_0);
    CloseHandle(visit_thread);
    CloseHandle(s_slow_visitor_entered);
    assert(append_latency_ms < 80U);

    printf("Flash stress passed: %u config writes + %u config reads + %u "
           "logs; append latency during slow visitor=%llu ms\n",
           CONFIG_WRITER_COUNT * CONFIG_WRITES_PER_THREAD,
           CONFIG_READ_COUNT, LOG_WRITER_COUNT * LOGS_PER_THREAD,
           (unsigned long long)append_latency_ms);
    free(s_flash_memory);
    DeleteCriticalSection(&s_flash_mutex);
    return 0;
}
