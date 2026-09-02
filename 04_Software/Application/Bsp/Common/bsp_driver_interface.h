#ifndef BSP_DRIVER_INTERFACE_H
#define BSP_DRIVER_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    BSP_IO_OK = 0,
    BSP_IO_ERROR,
    BSP_IO_TIMEOUT,
    BSP_IO_BUSY
} bsp_io_status_t;

typedef struct
{
    void *context;
    bsp_io_status_t (*lock)(void *context, uint32_t timeout_ms);
    void (*unlock)(void *context);
    /* address is the unshifted 7-bit I2C address; register size is in bytes. */
    bsp_io_status_t (*mem_read)(void *context, uint8_t address,
                                uint16_t register_address,
                                uint8_t register_address_size,
                                uint8_t *data, size_t size,
                                uint32_t timeout_ms);
    bsp_io_status_t (*mem_write)(void *context, uint8_t address,
                                 uint16_t register_address,
                                 uint8_t register_address_size,
                                 const uint8_t *data, size_t size,
                                 uint32_t timeout_ms);
} bsp_i2c_interface_t;

typedef struct
{
    void *context;
    bsp_io_status_t (*lock)(void *context, uint32_t timeout_ms);
    void (*unlock)(void *context);
    bsp_io_status_t (*write)(void *context, const uint8_t *data,
                             size_t size, uint32_t timeout_ms);
    bsp_io_status_t (*write_async)(void *context, const uint8_t *data,
                                   size_t size);
    bsp_io_status_t (*wait_complete)(void *context, uint32_t timeout_ms);
} bsp_spi_interface_t;

typedef struct
{
    void *context;
    void (*delay_ms)(void *context, uint32_t milliseconds);
} bsp_delay_interface_t;

typedef struct
{
    void *context;
    uint32_t (*get_tick_ms)(void *context);
} bsp_timebase_interface_t;

#endif /* BSP_DRIVER_INTERFACE_H */
