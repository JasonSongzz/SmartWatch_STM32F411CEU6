#ifndef DRV_ADAPTER_PORT_TEMPHUMI_H
#define DRV_ADAPTER_PORT_TEMPHUMI_H

#include <stdbool.h>
#include "drv_adapter_temphumi.h"

typedef struct {
    void *iic_bus;
    float temp_scale;
} temphumi_port_config_t;

bool drv_adapter_temphumi_register(uint32_t index, const temphumi_port_config_t *config);

#endif
