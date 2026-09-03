#include "bsp_spiflash_driver.h"

#include "sfud.h"

#include <stdarg.h>
#include <stdio.h>

#define SPIFLASH_LOCK_WAIT_FOREVER 0xFFFFFFFFUL
#define SPIFLASH_BUSY_RETRY_COUNT  10000U
#define SPIFLASH_RETRY_INTERVAL_MS 1U

static bsp_spiflash_driver_t *s_active_driver;

static sfud_flash *spiflash_device(const bsp_spiflash_driver_t *driver)
{
    return driver != NULL ? (sfud_flash *)driver->device : NULL;
}

static sfud_err spiflash_spi_write_read(const sfud_spi *spi,
                                        const uint8_t *write_buffer,
                                        size_t write_size,
                                        uint8_t *read_buffer,
                                        size_t read_size)
{
    bsp_spiflash_driver_t *driver =
        spi != NULL ? (bsp_spiflash_driver_t *)spi->user_data : NULL;
    spiflash_status_t status;

    if (driver == NULL || driver->spi.pf_write_read == NULL)
        return SFUD_ERR_NOT_FOUND;
    if (driver->lock_failed) return SFUD_ERR_TIMEOUT;

    status = driver->spi.pf_write_read(driver->spi.bus_context,
                                       write_buffer, write_size,
                                       read_buffer, read_size);
    if (status == SPIFLASH_OK) return SFUD_SUCCESS;
    if (status == SPIFLASH_ERROR_TIMEOUT) return SFUD_ERR_TIMEOUT;
    return read_size > 0U ? SFUD_ERR_READ : SFUD_ERR_WRITE;
}

static void spiflash_spi_lock(const sfud_spi *spi)
{
    bsp_spiflash_driver_t *driver =
        spi != NULL ? (bsp_spiflash_driver_t *)spi->user_data : NULL;

    if (driver == NULL || driver->spi.pf_lock == NULL) return;
    driver->lock_failed =
        driver->spi.pf_lock(driver->spi.bus_context,
                            SPIFLASH_LOCK_WAIT_FOREVER) != SPIFLASH_OK;
}

static void spiflash_spi_unlock(const sfud_spi *spi)
{
    bsp_spiflash_driver_t *driver =
        spi != NULL ? (bsp_spiflash_driver_t *)spi->user_data : NULL;

    if (driver == NULL || driver->spi.pf_unlock == NULL) return;
    if (!driver->lock_failed)
        (void)driver->spi.pf_unlock(driver->spi.bus_context);
    driver->lock_failed = false;
}

static void spiflash_retry_delay(void)
{
    if (s_active_driver != NULL &&
        s_active_driver->yield.pf_rtos_yield != NULL) {
        s_active_driver->yield.pf_rtos_yield(SPIFLASH_RETRY_INTERVAL_MS);
    }
}

#ifdef SFUD_USING_QSPI
static sfud_err spiflash_qspi_read(const sfud_spi *spi, uint32_t address,
                                   sfud_qspi_read_cmd_format *format,
                                   uint8_t *read_buffer, size_t read_size)
{
    (void)spi;
    (void)address;
    (void)format;
    (void)read_buffer;
    (void)read_size;
    return SFUD_ERR_NOT_FOUND;
}
#endif

sfud_err sfud_spi_port_init(sfud_flash *flash)
{
    bsp_spiflash_driver_t *driver = s_active_driver;

    if (flash == NULL || flash->index != SFUD_SPI_FLASH_DEVICE_INDEX ||
        driver == NULL || driver->spi.pf_spi_init == NULL ||
        driver->spi.pf_write_read == NULL || driver->spi.pf_lock == NULL ||
        driver->spi.pf_unlock == NULL ||
        driver->yield.pf_rtos_yield == NULL) {
        return SFUD_ERR_NOT_FOUND;
    }
    if (driver->spi.pf_spi_init(driver->spi.bus_context) != SPIFLASH_OK)
        return SFUD_ERR_NOT_FOUND;

    flash->spi.wr = spiflash_spi_write_read;
    flash->spi.lock = spiflash_spi_lock;
    flash->spi.unlock = spiflash_spi_unlock;
    flash->spi.user_data = driver;
#ifdef SFUD_USING_QSPI
    flash->spi.qspi_read = spiflash_qspi_read;
#endif
    flash->retry.times = SPIFLASH_BUSY_RETRY_COUNT;
    flash->retry.delay = spiflash_retry_delay;
    return SFUD_SUCCESS;
}

void sfud_log_debug(const char *file, const long line, const char *format, ...)
{
    char message[256];
    va_list args;

    va_start(args, format);
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    printf("[SFUD](%s:%ld) %s\n", file, line, message);
}

void sfud_log_info(const char *format, ...)
{
    char message[256];
    va_list args;

    va_start(args, format);
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    printf("[SFUD]%s\n", message);
}

static spiflash_status_t spiflash_map_error(sfud_err error)
{
    switch (error) {
    case SFUD_SUCCESS:
        return SPIFLASH_OK;
    case SFUD_ERR_NOT_FOUND:
        return SPIFLASH_ERROR_NOT_FOUND;
    case SFUD_ERR_ADDR_OUT_OF_BOUND:
        return SPIFLASH_ERROR_OUT_OF_RANGE;
    case SFUD_ERR_TIMEOUT:
        return SPIFLASH_ERROR_TIMEOUT;
    case SFUD_ERR_WRITE:
    case SFUD_ERR_READ:
    default:
        return SPIFLASH_ERROR_IO;
    }
}

static bool spiflash_range_valid(const sfud_flash *flash, uint32_t address,
                                 size_t size)
{
    return flash != NULL && (size_t)address <= flash->chip.capacity &&
           size <= flash->chip.capacity - (size_t)address;
}

spiflash_status_t spiflash_inst(
    bsp_spiflash_driver_t *driver,
    const spiflash_spi_driver_interface_t *spi,
    const spiflash_yield_interface_t *yield)
{
    sfud_flash *flash;
    sfud_err error;

    if (driver == NULL || spi == NULL || yield == NULL ||
        spi->pf_spi_init == NULL || spi->pf_write_read == NULL ||
        spi->pf_lock == NULL || spi->pf_unlock == NULL ||
        yield->pf_rtos_yield == NULL) {
        return SPIFLASH_ERROR_PARAMETER;
    }
    if (spiflash_is_ready(driver)) return SPIFLASH_OK;

    driver->device = NULL;
    driver->spi = *spi;
    driver->yield = *yield;
    driver->lock_failed = false;
    s_active_driver = driver;
    error = sfud_init();
    if (error != SFUD_SUCCESS) return spiflash_map_error(error);

    flash = sfud_get_device(SFUD_SPI_FLASH_DEVICE_INDEX);
    if (flash == NULL || !flash->init_ok) return SPIFLASH_ERROR_NOT_FOUND;

    driver->device = flash;
    return SPIFLASH_OK;
}

bool spiflash_is_ready(const bsp_spiflash_driver_t *driver)
{
    sfud_flash *flash = spiflash_device(driver);

    return flash != NULL && flash->init_ok;
}

spiflash_status_t spiflash_get_info(bsp_spiflash_driver_t *driver,
                                    spiflash_info_t *info)
{
    sfud_flash *flash = spiflash_device(driver);

    if (driver == NULL || info == NULL) return SPIFLASH_ERROR_PARAMETER;
    if (!spiflash_is_ready(driver)) return SPIFLASH_ERROR_NOT_READY;

    info->capacity = flash->chip.capacity;
    info->erase_size = flash->chip.erase_gran;
    info->write_granularity_bits = 1U;
    return SPIFLASH_OK;
}

spiflash_status_t spiflash_read(bsp_spiflash_driver_t *driver,
                                uint32_t address, void *buffer, size_t size)
{
    sfud_flash *flash = spiflash_device(driver);

    if (!spiflash_is_ready(driver)) return SPIFLASH_ERROR_NOT_READY;
    if (!spiflash_range_valid(flash, address, size))
        return SPIFLASH_ERROR_OUT_OF_RANGE;
    if (size == 0U) return SPIFLASH_OK;
    if (buffer == NULL) return SPIFLASH_ERROR_PARAMETER;

    return spiflash_map_error(sfud_read(flash, address, size, buffer));
}

spiflash_status_t spiflash_write(bsp_spiflash_driver_t *driver,
                                 uint32_t address, const void *buffer,
                                 size_t size)
{
    sfud_flash *flash = spiflash_device(driver);

    if (!spiflash_is_ready(driver)) return SPIFLASH_ERROR_NOT_READY;
    if (!spiflash_range_valid(flash, address, size))
        return SPIFLASH_ERROR_OUT_OF_RANGE;
    if (size == 0U) return SPIFLASH_OK;
    if (buffer == NULL) return SPIFLASH_ERROR_PARAMETER;

    return spiflash_map_error(sfud_write(flash, address, size, buffer));
}

spiflash_status_t spiflash_erase(bsp_spiflash_driver_t *driver,
                                 uint32_t address, size_t size)
{
    sfud_flash *flash = spiflash_device(driver);
    uint32_t erase_size;

    if (!spiflash_is_ready(driver)) return SPIFLASH_ERROR_NOT_READY;
    if (!spiflash_range_valid(flash, address, size))
        return SPIFLASH_ERROR_OUT_OF_RANGE;
    if (size == 0U) return SPIFLASH_OK;

    erase_size = flash->chip.erase_gran;
    if (erase_size == 0U || (address % erase_size) != 0U ||
        (size % erase_size) != 0U) {
        return SPIFLASH_ERROR_PARAMETER;
    }

    return spiflash_map_error(sfud_erase(flash, address, size));
}
