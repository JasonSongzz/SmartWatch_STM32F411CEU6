#include "drv_adapter_touch.h"

#include <stddef.h>

cst816t_status_t drv_adapter_touch_init(touch_drv_t *touch,
                                        const cst816t_iic_driver_interface_t *i2c,
                                        const cst816t_control_interface_t *control,
                                        const cst816t_delay_interface_t *delay,
                                        uint16_t width, uint16_t height)
{
    if (touch == NULL) return CST816T_ERROR_PARAM;
    return touch_handler_init(&touch->handler, i2c, control, delay, width, height);
}

cst816t_status_t drv_adapter_touch_read(touch_drv_t *touch, cst816t_point_t *point)
{
    return touch_handler_read(touch != NULL ? &touch->handler : NULL, point);
}
