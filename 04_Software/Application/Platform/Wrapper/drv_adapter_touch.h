#ifndef DRV_ADAPTER_TOUCH_H
#define DRV_ADAPTER_TOUCH_H

#include "bsp_touch_xxx_handler.h"
#include <stdbool.h>

typedef struct { bsp_touch_handler_t handler; } touch_drv_t;
cst816t_status_t drv_adapter_touch_init(touch_drv_t *touch,
                                        const cst816t_iic_driver_interface_t *i2c,
                                        const cst816t_control_interface_t *control,
                                        const cst816t_delay_interface_t *delay,
                                        uint16_t width, uint16_t height);
cst816t_status_t drv_adapter_touch_read(touch_drv_t *touch, cst816t_point_t *point);

#endif /* DRV_ADAPTER_TOUCH_H */
