#ifndef BSP_TOUCH_HANDLER_H
#define BSP_TOUCH_HANDLER_H

/* Change only this include when replacing the touch controller. */
#include "bsp_cst816t_driver.h"

#include <stdbool.h>

typedef struct
{
    bsp_touch_driver_t driver;
    bool initialized;
} bsp_touch_handler_t;

touch_status_t touch_handler_init(
    bsp_touch_handler_t *handler, const touch_iic_interface_t *i2c,
    const touch_control_interface_t *control,
    const touch_yield_interface_t *yield);
touch_status_t touch_handler_read(bsp_touch_handler_t *handler,
                                  touch_point_t *point);
touch_status_t touch_handler_get_info(const bsp_touch_handler_t *handler,
                                      touch_info_t *info);
touch_status_t touch_handler_sleep(bsp_touch_handler_t *handler);
touch_status_t touch_handler_wakeup(bsp_touch_handler_t *handler);

#endif /* BSP_TOUCH_HANDLER_H */
