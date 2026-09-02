#ifndef DRV_ADAPTER_PORT_I2C_H
#define DRV_ADAPTER_PORT_I2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "iic_hal.h"
#include "osal.h"

typedef struct
{
    iic_bus_t bus;
    osal_mutex_handle_t mutex;
    bool initialized;
} drv_soft_i2c_port_t;

bool drv_adapter_port_soft_i2c_init(drv_soft_i2c_port_t *port,
                                    const iic_bus_t *bus);
bool drv_adapter_port_soft_i2c_lock(drv_soft_i2c_port_t *port,
                                    uint32_t timeout_ms);
void drv_adapter_port_soft_i2c_unlock(drv_soft_i2c_port_t *port);
bool drv_adapter_port_soft_i2c_mem_read(drv_soft_i2c_port_t *port,
                                        uint8_t address, uint8_t reg,
                                        uint8_t *data, size_t size,
                                        uint32_t timeout_ms);
bool drv_adapter_port_soft_i2c_mem_write(drv_soft_i2c_port_t *port,
                                         uint8_t address, uint8_t reg,
                                         const uint8_t *data, size_t size,
                                         uint32_t timeout_ms);

#endif /* DRV_ADAPTER_PORT_I2C_H */
