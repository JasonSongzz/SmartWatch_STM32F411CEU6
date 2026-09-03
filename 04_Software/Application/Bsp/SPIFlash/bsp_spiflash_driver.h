#ifndef BSP_SPIFLASH_DRIVER_H
#define BSP_SPIFLASH_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SPIFLASH_OK = 0,
    SPIFLASH_ERROR_PARAMETER,
    SPIFLASH_ERROR_NOT_READY,
    SPIFLASH_ERROR_NOT_FOUND,
    SPIFLASH_ERROR_OUT_OF_RANGE,
    SPIFLASH_ERROR_TIMEOUT,
    SPIFLASH_ERROR_IO,
} spiflash_status_t;

typedef struct {
    uint32_t capacity;
    uint32_t erase_size;
    uint32_t write_granularity_bits;
} spiflash_info_t;

typedef struct {
    void *bus_context;
    spiflash_status_t (*pf_spi_init)(void *context);
    spiflash_status_t (*pf_write_read)(void *context,
                                       const uint8_t *write_buffer,
                                       size_t write_size,
                                       uint8_t *read_buffer,
                                       size_t read_size);
    spiflash_status_t (*pf_lock)(void *context, uint32_t timeout_ms);
    spiflash_status_t (*pf_unlock)(void *context);
} spiflash_spi_driver_interface_t;

typedef struct {
    void (*pf_rtos_yield)(uint32_t milliseconds);
} spiflash_yield_interface_t;

typedef struct {
    void *device;
    spiflash_spi_driver_interface_t spi;
    spiflash_yield_interface_t yield;
    bool lock_failed;
} bsp_spiflash_driver_t;

spiflash_status_t spiflash_inst(
    bsp_spiflash_driver_t *driver,
    const spiflash_spi_driver_interface_t *spi,
    const spiflash_yield_interface_t *yield);
bool spiflash_is_ready(const bsp_spiflash_driver_t *driver);
spiflash_status_t spiflash_get_info(bsp_spiflash_driver_t *driver,
                                    spiflash_info_t *info);
spiflash_status_t spiflash_read(bsp_spiflash_driver_t *driver,
                                uint32_t address, void *buffer, size_t size);
spiflash_status_t spiflash_write(bsp_spiflash_driver_t *driver,
                                 uint32_t address, const void *buffer,
                                 size_t size);
spiflash_status_t spiflash_erase(bsp_spiflash_driver_t *driver,
                                 uint32_t address, size_t size);

#endif /* BSP_SPIFLASH_DRIVER_H */
