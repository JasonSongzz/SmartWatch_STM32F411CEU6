#ifndef DRV_ADAPTER_PORT_ACCEL_H
#define DRV_ADAPTER_PORT_ACCEL_H

#include <stdbool.h>
#include <stdint.h>

#include "drv_adapter_accel.h"

typedef struct
{
    void *iic_bus;
    float accel_scale;
} accel_port_config_t;

bool drv_adapter_port_accel_register(uint32_t index,
                                     const accel_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_ACCEL_H */
