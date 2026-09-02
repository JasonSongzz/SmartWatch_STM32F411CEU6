#ifndef BSP_TOUCH_HANDLER_H
#define BSP_TOUCH_HANDLER_H

#include "bsp_cst816t_driver.h"
#include <stdbool.h>

typedef struct
{
    bsp_cst816t_t driver;
    bool initialized;
} bsp_touch_handler_t;

cst816t_status_t touch_handler_init(bsp_touch_handler_t *handler,
                                    const cst816t_iic_driver_interface_t *i2c,
                                    const cst816t_control_interface_t *control,
                                    const cst816t_delay_interface_t *delay,
                                    uint16_t width, uint16_t height);
cst816t_status_t touch_handler_read(bsp_touch_handler_t *handler,
                                    cst816t_point_t *point);
cst816t_status_t touch_handler_sleep(bsp_touch_handler_t *handler);
cst816t_status_t touch_handler_wakeup(bsp_touch_handler_t *handler);

#endif /* BSP_TOUCH_HANDLER_H */
