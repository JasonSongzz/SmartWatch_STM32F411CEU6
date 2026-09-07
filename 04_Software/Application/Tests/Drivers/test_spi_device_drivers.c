#include "bsp_spiflash_driver.h"
#include "bsp_st7789t3_driver.h"
#include "sfud.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint8_t data[512];
    size_t size;
    bool dc_data;
} display_transfer_t;

typedef struct
{
    display_transfer_t transfer[1024];
    size_t transfer_count;
    uint32_t dma_count;
    uint32_t wait_count;
    uint32_t lock_count;
    uint32_t unlock_count;
    uint32_t lock_depth;
    uint32_t max_lock_depth;
    uint32_t delay_total_ms;
    bool cs_high;
    bool dc_data;
    bool reset_high;
    bool backlight_on;
    display_status_t sync_result;
    display_status_t dma_result;
    display_status_t wait_result;
} display_bus_t;

static display_status_t display_write(void *context, const uint8_t *data,
                                      size_t size, uint32_t timeout_ms)
{
    display_bus_t *bus = context;
    display_transfer_t *transfer;

    assert(timeout_ms == 1000U);
    assert(bus->transfer_count <
           sizeof(bus->transfer) / sizeof(bus->transfer[0]));
    assert(size <= sizeof(bus->transfer[0].data));
    transfer = &bus->transfer[bus->transfer_count++];
    transfer->size = size;
    transfer->dc_data = bus->dc_data;
    memcpy(transfer->data, data, size);
    return bus->sync_result;
}

static display_status_t display_write_dma(void *context, const uint8_t *data,
                                          size_t size)
{
    display_bus_t *bus = context;
    (void)data;
    assert(size > 0U);
    ++bus->dma_count;
    return bus->dma_result;
}

static display_status_t display_wait(void *context, uint32_t timeout_ms)
{
    display_bus_t *bus = context;
    assert(timeout_ms == 1000U);
    ++bus->wait_count;
    return bus->wait_result;
}

static display_status_t display_lock(void *context, uint32_t timeout_ms)
{
    display_bus_t *bus = context;
    assert(timeout_ms == 1000U);
    ++bus->lock_count;
    ++bus->lock_depth;
    if (bus->lock_depth > bus->max_lock_depth)
        bus->max_lock_depth = bus->lock_depth;
    return DISPLAY_OK;
}

static display_status_t display_unlock(void *context)
{
    display_bus_t *bus = context;
    assert(bus->lock_depth == 1U);
    --bus->lock_depth;
    ++bus->unlock_count;
    return DISPLAY_OK;
}

static void display_set_cs(void *context, bool high)
{
    ((display_bus_t *)context)->cs_high = high;
}

static void display_set_dc(void *context, bool data_mode)
{
    ((display_bus_t *)context)->dc_data = data_mode;
}

static void display_set_reset(void *context, bool high)
{
    ((display_bus_t *)context)->reset_high = high;
}

static void display_set_backlight(void *context, bool on)
{
    ((display_bus_t *)context)->backlight_on = on;
}

static void display_delay(void *context, uint32_t milliseconds)
{
    ((display_bus_t *)context)->delay_total_ms += milliseconds;
}

static void test_display_driver(void)
{
    display_bus_t bus = {
        .cs_high = true,
        .sync_result = DISPLAY_OK,
        .dma_result = DISPLAY_OK,
        .wait_result = DISPLAY_OK,
    };
    const display_spi_interface_t spi = {
        .bus_context = &bus,
        .pf_spi_write = display_write,
        .pf_spi_write_dma = display_write_dma,
        .pf_spi_wait_complete = display_wait,
        .pf_lock = display_lock,
        .pf_unlock = display_unlock,
    };
    const display_control_interface_t control = {
        .context = &bus,
        .pf_set_cs = display_set_cs,
        .pf_set_dc = display_set_dc,
        .pf_set_reset = display_set_reset,
        .pf_set_backlight = display_set_backlight,
    };
    const display_delay_interface_t delay = {
        .context = &bus,
        .pf_delay_ms = display_delay,
    };
    bsp_display_driver_t display = {0};
    display_info_t info;
    const uint8_t pixels[] = {0x12U, 0x34U, 0x56U, 0x78U};
    size_t start;

    assert(st7789t3_inst(NULL, &spi, &control, &delay) ==
           DISPLAY_ERROR_PARAMETER);
    assert(st7789t3_inst(&display, &spi, &control, &delay) == DISPLAY_OK);
    assert(display.initialized);
    assert(bus.delay_total_ms == 250U);
    assert(bus.reset_high && bus.backlight_on && bus.cs_high);
    assert(bus.lock_depth == 0U && bus.lock_count == bus.unlock_count);

    assert(display.pf_get_info(&display, &info) == DISPLAY_OK);
    assert(info.width == 240U && info.height == 280U);
    assert(info.bits_per_pixel == 16U && info.requires_byte_swap);
    start = bus.transfer_count;
    assert(display.pf_write_area(&display, 9U, 19U, 10U, 19U,
                                 pixels, sizeof(pixels)) == DISPLAY_OK);
    assert(bus.transfer_count == start + 5U);
    assert(!bus.transfer[start].dc_data &&
           bus.transfer[start].data[0] == ST7789T3_CMD_CASET);
    assert(bus.transfer[start + 1U].dc_data &&
           bus.transfer[start + 1U].size == 4U);
    assert(bus.transfer[start + 1U].data[0] == 0U &&
           bus.transfer[start + 1U].data[1] == 9U &&
           bus.transfer[start + 1U].data[2] == 0U &&
           bus.transfer[start + 1U].data[3] == 10U);
    assert(!bus.transfer[start + 2U].dc_data &&
           bus.transfer[start + 2U].data[0] == ST7789T3_CMD_RASET);
    assert(bus.transfer[start + 3U].dc_data &&
           bus.transfer[start + 3U].data[1] == 19U &&
           bus.transfer[start + 3U].data[3] == 19U);
    assert(!bus.transfer[start + 4U].dc_data &&
           bus.transfer[start + 4U].data[0] == ST7789T3_CMD_RAMWR);
    assert(bus.dma_count == 1U && bus.wait_count == 1U);
    assert(display.pf_write_area(&display, 20U, 10U, 19U, 20U,
                                 pixels, sizeof(pixels)) ==
           DISPLAY_ERROR_PARAMETER);
    assert(display.pf_write_area(&display, 0U, 0U, 240U, 1U,
                                 pixels, sizeof(pixels)) ==
           DISPLAY_ERROR_PARAMETER);
    assert(display.pf_write_area(&display, 0U, 0U, 0U, 0U,
                                 pixels, sizeof(pixels)) ==
           DISPLAY_ERROR_PARAMETER);
    assert(bus.cs_high && bus.lock_count == bus.unlock_count);

    bus.wait_result = DISPLAY_ERROR_TIMEOUT;
    assert(display.pf_write_area(&display, 9U, 19U, 10U, 19U,
                                 pixels, sizeof(pixels)) ==
           DISPLAY_ERROR_TIMEOUT);
    assert(bus.cs_high && bus.lock_depth == 0U &&
           bus.lock_count == bus.unlock_count);
    bus.wait_result = DISPLAY_OK;

    assert(display.pf_sleep(&display) == DISPLAY_OK);
    assert(!display.initialized && !bus.backlight_on);
    assert(display.pf_write_area(&display, 9U, 19U, 10U, 19U,
                                 pixels, sizeof(pixels)) ==
           DISPLAY_ERROR_PARAMETER);
    assert(display.pf_wakeup(&display) == DISPLAY_OK);
    assert(display.initialized && bus.backlight_on);
    assert(bus.lock_depth == 0U && bus.lock_count == bus.unlock_count);
}

typedef struct
{
    uint32_t init_count;
    uint32_t write_read_count;
    uint32_t lock_count;
    uint32_t unlock_count;
    uint32_t yield_count;
    bool lock_fail;
    spiflash_status_t transfer_result;
} flash_bus_t;

#define TEST_FLASH_CAPACITY (64U * 1024U)
#define TEST_ERASE_SIZE     4096U

static sfud_flash s_sfud_flash;
static uint8_t s_flash_memory[TEST_FLASH_CAPACITY];
static sfud_err s_sfud_init_result;
static sfud_err s_sfud_read_result;
static sfud_err s_sfud_write_result;
static sfud_err s_sfud_erase_result;

extern sfud_err sfud_spi_port_init(sfud_flash *flash);

sfud_err sfud_init(void)
{
    sfud_err result;

    if (s_sfud_init_result != SFUD_SUCCESS) return s_sfud_init_result;
    memset(&s_sfud_flash, 0, sizeof(s_sfud_flash));
    s_sfud_flash.index = SFUD_SPI_FLASH_DEVICE_INDEX;
    s_sfud_flash.chip.capacity = TEST_FLASH_CAPACITY;
    s_sfud_flash.chip.erase_gran = TEST_ERASE_SIZE;
    result = sfud_spi_port_init(&s_sfud_flash);
    s_sfud_flash.init_ok = result == SFUD_SUCCESS;
    return result;
}

sfud_flash *sfud_get_device(size_t index)
{
    return index == SFUD_SPI_FLASH_DEVICE_INDEX ? &s_sfud_flash : NULL;
}

sfud_err sfud_read(const sfud_flash *flash, uint32_t address, size_t size,
                   uint8_t *data)
{
    (void)flash;
    if (s_sfud_read_result != SFUD_SUCCESS) return s_sfud_read_result;
    memcpy(data, &s_flash_memory[address], size);
    return SFUD_SUCCESS;
}

sfud_err sfud_write(const sfud_flash *flash, uint32_t address, size_t size,
                    const uint8_t *data)
{
    size_t index;
    (void)flash;
    if (s_sfud_write_result != SFUD_SUCCESS) return s_sfud_write_result;
    for (index = 0U; index < size; ++index)
        s_flash_memory[address + index] &= data[index];
    return SFUD_SUCCESS;
}

sfud_err sfud_erase(const sfud_flash *flash, uint32_t address, size_t size)
{
    (void)flash;
    if (s_sfud_erase_result != SFUD_SUCCESS) return s_sfud_erase_result;
    memset(&s_flash_memory[address], 0xFF, size);
    return SFUD_SUCCESS;
}

static spiflash_status_t flash_spi_init(void *context)
{
    ++((flash_bus_t *)context)->init_count;
    return SPIFLASH_OK;
}

static spiflash_status_t flash_write_read(void *context,
                                          const uint8_t *write_buffer,
                                          size_t write_size,
                                          uint8_t *read_buffer,
                                          size_t read_size)
{
    flash_bus_t *bus = context;
    (void)write_buffer;
    (void)write_size;
    ++bus->write_read_count;
    if (read_buffer != NULL) memset(read_buffer, 0xA5, read_size);
    return bus->transfer_result;
}

static spiflash_status_t flash_lock(void *context, uint32_t timeout_ms)
{
    flash_bus_t *bus = context;
    assert(timeout_ms == UINT32_MAX);
    ++bus->lock_count;
    return bus->lock_fail ? SPIFLASH_ERROR_TIMEOUT : SPIFLASH_OK;
}

static spiflash_status_t flash_unlock(void *context)
{
    ++((flash_bus_t *)context)->unlock_count;
    return SPIFLASH_OK;
}

static flash_bus_t *s_flash_bus;

static void flash_yield(uint32_t milliseconds)
{
    assert(milliseconds == 1U);
    ++s_flash_bus->yield_count;
}

static void test_spiflash_driver(void)
{
    flash_bus_t bus = {.transfer_result = SPIFLASH_OK};
    const spiflash_spi_driver_interface_t spi = {
        .bus_context = &bus,
        .pf_spi_init = flash_spi_init,
        .pf_write_read = flash_write_read,
        .pf_lock = flash_lock,
        .pf_unlock = flash_unlock,
    };
    const spiflash_yield_interface_t yield = {.pf_rtos_yield = flash_yield};
    bsp_spiflash_driver_t driver = {0};
    spiflash_info_t info;
    uint8_t write_data[] = {0x12U, 0x34U, 0x56U};
    uint8_t read_data[sizeof(write_data)] = {0};
    uint8_t byte = 0U;

    memset(s_flash_memory, 0xFF, sizeof(s_flash_memory));
    s_flash_bus = &bus;
    assert(spiflash_inst(NULL, &spi, &yield) == SPIFLASH_ERROR_PARAMETER);
    assert(spiflash_inst(&driver, &spi, &yield) == SPIFLASH_OK);
    assert(bus.init_count == 1U && spiflash_is_ready(&driver));
    assert(spiflash_get_info(&driver, &info) == SPIFLASH_OK);
    assert(info.capacity == TEST_FLASH_CAPACITY &&
           info.erase_size == TEST_ERASE_SIZE &&
           info.write_granularity_bits == 1U);

    assert(spiflash_write(&driver, 100U, write_data, sizeof(write_data)) ==
           SPIFLASH_OK);
    assert(spiflash_read(&driver, 100U, read_data, sizeof(read_data)) ==
           SPIFLASH_OK);
    assert(memcmp(write_data, read_data, sizeof(write_data)) == 0);
    assert(spiflash_erase(&driver, 0U, TEST_ERASE_SIZE) == SPIFLASH_OK);
    assert(s_flash_memory[100] == 0xFFU);
    assert(spiflash_erase(&driver, 1U, TEST_ERASE_SIZE) ==
           SPIFLASH_ERROR_PARAMETER);
    assert(spiflash_read(&driver, TEST_FLASH_CAPACITY, &byte, 1U) ==
           SPIFLASH_ERROR_OUT_OF_RANGE);
    assert(spiflash_read(&driver, TEST_FLASH_CAPACITY, NULL, 0U) ==
           SPIFLASH_OK);
    assert(spiflash_write(&driver, 0U, NULL, 1U) ==
           SPIFLASH_ERROR_PARAMETER);

    s_sfud_read_result = SFUD_ERR_TIMEOUT;
    assert(spiflash_read(&driver, 0U, &byte, 1U) == SPIFLASH_ERROR_TIMEOUT);
    s_sfud_read_result = SFUD_SUCCESS;
    s_sfud_write_result = SFUD_ERR_WRITE;
    assert(spiflash_write(&driver, 0U, &byte, 1U) == SPIFLASH_ERROR_IO);
    s_sfud_write_result = SFUD_SUCCESS;

    bus.lock_fail = true;
    s_sfud_flash.spi.lock(&s_sfud_flash.spi);
    assert(s_sfud_flash.spi.wr(&s_sfud_flash.spi, &byte, 1U, NULL, 0U) ==
           SFUD_ERR_TIMEOUT);
    s_sfud_flash.spi.unlock(&s_sfud_flash.spi);
    assert(bus.lock_count == 1U && bus.unlock_count == 0U);
    bus.lock_fail = false;
    s_sfud_flash.spi.lock(&s_sfud_flash.spi);
    assert(s_sfud_flash.spi.wr(&s_sfud_flash.spi, &byte, 1U, read_data,
                              sizeof(read_data)) == SFUD_SUCCESS);
    s_sfud_flash.spi.unlock(&s_sfud_flash.spi);
    assert(bus.lock_count == 2U && bus.unlock_count == 1U);
    assert(bus.write_read_count == 1U);

    s_sfud_flash.retry.delay();
    assert(bus.yield_count == 1U && s_sfud_flash.retry.times == 10000U);
}

int main(void)
{
    test_display_driver();
    test_spiflash_driver();
    puts("SPI display/flash driver protocol tests passed");
    return 0;
}
