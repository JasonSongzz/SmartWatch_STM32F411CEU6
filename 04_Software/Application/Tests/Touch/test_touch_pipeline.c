#include "bsp_cst816t_driver.h"
#include "drv_adapter_port_touch.h"
#include "drv_adapter_touch.h"
#include "iic_hal.h"
#include "osal.h"

#include <assert.h>
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

#define READER_COUNT 4U
#define READ_COUNT   2000U
#define CONFIG_COUNT 500U

typedef struct { CRITICAL_SECTION native; } test_mutex_t;

static GPIO_TypeDef s_sda_gpio, s_scl_gpio, s_reset_gpio;
static uint8_t s_reg, s_send_index, s_receive_index;
static uint32_t s_sample_index;
static volatile LONG s_failures, s_reads;
static volatile LONG s_active_transactions, s_max_transactions;
static volatile LONG s_seen_sequences[READER_COUNT * READ_COUNT + 1U];

int32_t osal_mutex_create(osal_mutex_handle_t *handle)
{
    test_mutex_t *mutex;
    if (handle == NULL) return OSAL_INVALID_POINTER;
    mutex = malloc(sizeof(*mutex));
    if (mutex == NULL) return OSAL_ERROR;
    InitializeCriticalSection(&mutex->native);
    *handle = mutex;
    return OSAL_SUCCESS;
}

void osal_mutex_delete(osal_mutex_handle_t handle)
{
    test_mutex_t *mutex = handle;
    if (mutex != NULL) {
        DeleteCriticalSection(&mutex->native);
        free(mutex);
    }
}

int32_t osal_mutex_take(osal_mutex_handle_t handle, osal_time_ms_t timeout_ms)
{
    test_mutex_t *mutex = handle;
    (void)timeout_ms;
    if (mutex == NULL) return OSAL_INVALID_POINTER;
    EnterCriticalSection(&mutex->native);
    return OSAL_SUCCESS;
}

int32_t osal_mutex_give(osal_mutex_handle_t handle)
{
    test_mutex_t *mutex = handle;
    if (mutex == NULL) return OSAL_INVALID_POINTER;
    LeaveCriticalSection(&mutex->native);
    return OSAL_SUCCESS;
}

void osal_task_delay_ms(uint32_t milliseconds)
{
    (void)milliseconds;
    SwitchToThread();
}

osal_time_ms_t osal_time_get_ms(void) { return (osal_time_ms_t)GetTickCount(); }

void HAL_GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *init)
{
    (void)gpio; (void)init;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *gpio, uint16_t pin, GPIO_PinState state)
{
    (void)gpio; (void)pin; (void)state;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *gpio, uint16_t pin)
{
    (void)gpio; (void)pin;
    return GPIO_PIN_RESET;
}

void IICInit(iic_bus_t *bus) { (void)bus; }

void IICStart(iic_bus_t *bus)
{
    LONG active;
    (void)bus;
    if (s_send_index != 0U) return;
    active = InterlockedIncrement(&s_active_transactions);
    for (;;) {
        LONG maximum = s_max_transactions;
        if (active <= maximum ||
            InterlockedCompareExchange(&s_max_transactions, active, maximum) ==
                maximum)
            break;
    }
}

void IICStop(iic_bus_t *bus)
{
    (void)bus;
    s_send_index = 0U;
    s_receive_index = 0U;
    InterlockedDecrement(&s_active_transactions);
}

unsigned char IICWaitAck(iic_bus_t *bus) { (void)bus; return SUCCESS; }
void IICSendAck(iic_bus_t *bus) { (void)bus; }
void IICSendNotAck(iic_bus_t *bus) { (void)bus; }

void IICSendByte(iic_bus_t *bus, unsigned char value)
{
    (void)bus;
    if (s_send_index == 1U) s_reg = value;
    ++s_send_index;
}

unsigned char IICReceiveByte(iic_bus_t *bus)
{
    uint8_t value;
    uint16_t x = (uint16_t)(20U + (s_sample_index % 180U));
    uint16_t y = (uint16_t)(30U + (s_sample_index % 220U));
    (void)bus;

    if (s_reg == CST816T_REG_CHIP_ID) value = 0xB5U;
    else if (s_reg == CST816T_REG_VERSION) value = 0x11U;
    else {
        switch (s_receive_index) {
        case 0U: value = 0U; break;
        case 1U: value = 1U; break;
        case 2U: value = (uint8_t)((x >> 8U) & 0x0FU); break;
        case 3U: value = (uint8_t)x; break;
        case 4U: value = (uint8_t)((y >> 8U) & 0x0FU); break;
        default: value = (uint8_t)y; ++s_sample_index; break;
        }
    }
    ++s_receive_index;
    return value;
}

static DWORD WINAPI reader_thread(LPVOID argument)
{
    uint32_t index;
    (void)argument;
    for (index = 0U; index < READ_COUNT; ++index) {
        drv_adapter_touch_point_t point;
        drv_adapter_touch_status_t status = drv_adapter_touch_read(0U, &point);
        if (status != DRV_ADAPTER_TOUCH_OK || point.fingers != 1U ||
            point.x >= 240U || point.y >= 280U || point.sequence == 0U) {
            InterlockedIncrement(&s_failures);
        } else if (point.sequence > READER_COUNT * READ_COUNT ||
                   InterlockedCompareExchange(
                       &s_seen_sequences[point.sequence], 1, 0) != 0) {
                InterlockedIncrement(&s_failures);
        }
        InterlockedIncrement(&s_reads);
        SwitchToThread();
    }
    return 0U;
}

static DWORD WINAPI config_thread(LPVOID argument)
{
    uint32_t index;
    (void)argument;
    for (index = 0U; index < CONFIG_COUNT; ++index) {
        drv_adapter_touch_processing_config_t config;
        if (!drv_adapter_touch_get_processing_config(0U, &config)) {
            InterlockedIncrement(&s_failures);
            continue;
        }
        config.invert_x = !config.invert_x;
        if (!drv_adapter_touch_set_processing_config(0U, &config))
            InterlockedIncrement(&s_failures);
        SwitchToThread();
    }
    return 0U;
}

int main(void)
{
    iic_bus_t bus = {&s_sda_gpio, &s_scl_gpio, GPIO_PIN_1, GPIO_PIN_2};
    touch_port_config_t config = {
        .iic_bus = &bus,
        .reset_port = &s_reset_gpio,
        .reset_pin = GPIO_PIN_3,
        .interrupt_active_low = true,
    };
    drv_adapter_touch_info_t info;
    HANDLE threads[READER_COUNT + 1U];
    uint32_t index;

    assert(drv_adapter_port_touch_register(0U, &config));
    assert(drv_adapter_touch_init(0U));
    assert(drv_adapter_touch_get_info(0U, &info));
    assert(info.width == 240U && info.height == 280U && info.max_points == 1U);
    for (index = 0U; index < READER_COUNT; ++index)
        threads[index] = CreateThread(NULL, 0, reader_thread, NULL, 0, NULL);
    threads[READER_COUNT] = CreateThread(NULL, 0, config_thread, NULL, 0, NULL);
    assert(WaitForMultipleObjects(READER_COUNT + 1U, threads, TRUE, 60000U) ==
           WAIT_OBJECT_0);
    for (index = 0U; index < READER_COUNT + 1U; ++index)
        CloseHandle(threads[index]);
    assert(s_reads == (LONG)(READER_COUNT * READ_COUNT));
    assert(s_failures == 0);
    assert(s_max_transactions == 1 && s_active_transactions == 0);
    assert(drv_adapter_touch_sleep(0U));
    assert(drv_adapter_touch_wakeup(0U));
    printf("Touch stress passed: %ld reads, %u config switches, max I2C "
           "transactions=%ld\n", s_reads, CONFIG_COUNT, s_max_transactions);
    return 0;
}
