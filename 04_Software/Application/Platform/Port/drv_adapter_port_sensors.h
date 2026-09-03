#ifndef DRV_ADAPTER_PORT_SENSORS_H
#define DRV_ADAPTER_PORT_SENSORS_H

#include <stdbool.h>

/* Registers the three default sensor ports. It does not access the devices. */
bool drv_adapter_port_sensors_register_defaults(void);

#endif /* DRV_ADAPTER_PORT_SENSORS_H */
