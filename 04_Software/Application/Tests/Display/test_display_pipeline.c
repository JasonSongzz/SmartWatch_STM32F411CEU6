#include "bsp_display_handler.h"
#include "drv_adapter_display.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define STRESS_ITERATIONS 5000U

typedef struct
{
    CRITICAL_SECTION mutex;
    uint8_t last_command;
    uint8_t active_x;
    volatile LONG pixel_writes;
    volatile LONG mismatched_writes;
    bool dc_data;
} display_test_bus_t;

typedef struct
{
    bsp_display_handler_t handler;
    display_test_bus_t bus;
    display_spi_interface_t spi;
    display_control_interface_t control;
    display_delay_interface_t delay;
} display_test_port_t;

static display_test_port_t s_port;

static display_status_t test_spi_write(void *context, const uint8_t *data,
                                       size_t size, uint32_t timeout_ms)
{
    display_test_bus_t *bus = context;
    (void)timeout_ms;

    if (data == NULL || size == 0U) return DISPLAY_ERROR_PARAMETER;
    if (!bus->dc_data)
        bus->last_command = data[0];
    else if (bus->last_command == ST7789T3_CMD_CASET && size == 4U)
        bus->active_x = data[1];
    return DISPLAY_OK;
}

static display_status_t test_spi_write_dma(void *context,
                                           const uint8_t *data, size_t size)
{
    display_test_bus_t *bus = context;

    if (data == NULL || size == 0U) return DISPLAY_ERROR_PARAMETER;
    InterlockedIncrement(&bus->pixel_writes);
    if (data[0] != bus->active_x)
        InterlockedIncrement(&bus->mismatched_writes);
    return DISPLAY_OK;
}

static display_status_t test_spi_wait(void *context, uint32_t timeout_ms)
{
    (void)context;
    (void)timeout_ms;
    return DISPLAY_OK;
}

static display_status_t test_lock(void *context, uint32_t timeout_ms)
{
    (void)timeout_ms;
    EnterCriticalSection(&((display_test_bus_t *)context)->mutex);
    return DISPLAY_OK;
}

static display_status_t test_unlock(void *context)
{
    LeaveCriticalSection(&((display_test_bus_t *)context)->mutex);
    return DISPLAY_OK;
}

static void test_cs(void *context, bool high)
{
    (void)context;
    (void)high;
}

static void test_dc(void *context, bool data_mode)
{
    ((display_test_bus_t *)context)->dc_data = data_mode;
}

static void test_reset(void *context, bool high)
{
    (void)context;
    (void)high;
}

static void test_backlight(void *context, bool on)
{
    (void)context;
    (void)on;
}

static void test_delay(void *context, uint32_t milliseconds)
{
    (void)context;
    (void)milliseconds;
}

static bool port_init(display_drv_t *dev)
{
    display_test_port_t *port = dev->user_data;
    return display_handler_init(&port->handler, &port->spi, &port->control,
                                &port->delay) == DISPLAY_OK;
}

static bool port_write_area(display_drv_t *dev, uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1,
                            const uint8_t *pixels, size_t size)
{
    display_test_port_t *port = dev->user_data;
    return display_handler_write_area(&port->handler, x0, y0, x1, y1,
                                      pixels, size) == DISPLAY_OK;
}

static bool port_fill(display_drv_t *dev, uint16_t color)
{
    display_test_port_t *port = dev->user_data;
    return display_handler_fill(&port->handler, color) == DISPLAY_OK;
}

static bool port_get_info(display_drv_t *dev, drv_adapter_display_info_t *info)
{
    display_test_port_t *port = dev->user_data;
    display_info_t bsp_info;
    if (display_handler_get_info(&port->handler, &bsp_info) != DISPLAY_OK)
        return false;
    info->width = bsp_info.width;
    info->height = bsp_info.height;
    info->x_offset = bsp_info.x_offset;
    info->y_offset = bsp_info.y_offset;
    info->bits_per_pixel = bsp_info.bits_per_pixel;
    info->requires_byte_swap = bsp_info.requires_byte_swap;
    return true;
}

static bool port_sleep(display_drv_t *dev)
{
    return display_handler_sleep(
               &((display_test_port_t *)dev->user_data)->handler) == DISPLAY_OK;
}

static bool port_wakeup(display_drv_t *dev)
{
    return display_handler_wakeup(
               &((display_test_port_t *)dev->user_data)->handler) == DISPLAY_OK;
}

static DWORD WINAPI writer_a(LPVOID parameter)
{
    uint32_t index;
    const uint8_t pixels[] = {1U, 0U};
    (void)parameter;

    for (index = 0U; index < STRESS_ITERATIONS; ++index)
    {
        assert(drv_adapter_display_write_area(0U, 1U, 0U, 1U, 0U,
                                              pixels, sizeof(pixels)));
        SwitchToThread();
    }
    return 0U;
}

static DWORD WINAPI writer_b(LPVOID parameter)
{
    uint32_t index;
    const uint8_t pixels[] = {2U, 0U};
    (void)parameter;

    for (index = 0U; index < STRESS_ITERATIONS; ++index)
    {
        assert(drv_adapter_display_write_area(0U, 2U, 0U, 2U, 0U,
                                              pixels, sizeof(pixels)));
        SwitchToThread();
    }
    return 0U;
}

int main(void)
{
    display_drv_t driver;
    drv_adapter_display_info_t info;
    HANDLE threads[2];

    memset(&s_port, 0, sizeof(s_port));
    InitializeCriticalSection(&s_port.bus.mutex);
    s_port.spi = (display_spi_interface_t){
        .bus_context = &s_port.bus,
        .pf_spi_write = test_spi_write,
        .pf_spi_write_dma = test_spi_write_dma,
        .pf_spi_wait_complete = test_spi_wait,
        .pf_lock = test_lock,
        .pf_unlock = test_unlock,
    };
    s_port.control = (display_control_interface_t){
        .context = &s_port.bus,
        .pf_set_cs = test_cs,
        .pf_set_dc = test_dc,
        .pf_set_reset = test_reset,
        .pf_set_backlight = test_backlight,
    };
    s_port.delay = (display_delay_interface_t){
        .context = &s_port.bus,
        .pf_delay_ms = test_delay,
    };
    driver = (display_drv_t){
        .user_data = &s_port,
        .init = port_init,
        .write_area = port_write_area,
        .fill = port_fill,
        .get_info = port_get_info,
        .sleep = port_sleep,
        .wakeup = port_wakeup,
    };
    assert(drv_adapter_display_reg(0U, &driver));
    assert(drv_adapter_display_init(0U));
    assert(drv_adapter_display_get_info(0U, &info));
    assert(info.width == 240U && info.height == 280U);

    threads[0] = CreateThread(NULL, 0, writer_a, NULL, 0, NULL);
    threads[1] = CreateThread(NULL, 0, writer_b, NULL, 0, NULL);
    assert(threads[0] != NULL && threads[1] != NULL);
    assert(WaitForMultipleObjects(2, threads, TRUE, 30000U) == WAIT_OBJECT_0);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    assert(s_port.bus.pixel_writes == (LONG)(STRESS_ITERATIONS * 2U));
    assert(s_port.bus.mismatched_writes == 0);
    printf("Display atomic-area stress passed: %ld transfers, mismatches=%ld\n",
           s_port.bus.pixel_writes, s_port.bus.mismatched_writes);
    DeleteCriticalSection(&s_port.bus.mutex);
    return 0;
}
