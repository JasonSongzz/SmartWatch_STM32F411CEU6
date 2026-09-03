#include "drv_adapter_port_sensors.h"

#include "drv_adapter_port_accel.h"
#include "drv_adapter_port_temphumi.h"
#include "drv_adapter_port_touch.h"

#include <stddef.h>

#define DEFAULT_SENSOR_INDEX (0U)

bool drv_adapter_port_sensors_register_defaults(void)
{
    if (!drv_adapter_port_temphumi_register(DEFAULT_SENSOR_INDEX, NULL))
        return false;

    if (!drv_adapter_port_accel_register(DEFAULT_SENSOR_INDEX, NULL))
        return false;

    return drv_adapter_port_touch_register(DEFAULT_SENSOR_INDEX, NULL);
}
