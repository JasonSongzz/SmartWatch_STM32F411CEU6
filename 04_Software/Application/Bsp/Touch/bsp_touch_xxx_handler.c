#include "bsp_touch_xxx_handler.h"

#include <stddef.h>

cst816t_status_t touch_handler_init(bsp_touch_handler_t *handler,
                                    const cst816t_iic_driver_interface_t *i2c,
                                    const cst816t_control_interface_t *control,
                                    const cst816t_delay_interface_t *delay,
                                    uint16_t width, uint16_t height)
{
    cst816t_status_t status;

    if (handler == NULL || i2c == NULL || control == NULL || delay == NULL)
        return CST816T_ERROR_PARAM;

    if (handler->initialized) return CST816T_OK;

    status = cst816t_inst(&handler->driver, i2c, control, delay, width, height);

    if (status == CST816T_OK) handler->initialized = true;

    return status;
}

cst816t_status_t touch_handler_read(bsp_touch_handler_t *handler,
                                    cst816t_point_t *point)
{
    if (handler == NULL || !handler->initialized || point == NULL ||
        handler->driver.pf_read_point == NULL) return CST816T_ERROR_RESOURCE;

    return handler->driver.pf_read_point(&handler->driver, point);
}

cst816t_status_t touch_handler_sleep(bsp_touch_handler_t *handler)
{
    cst816t_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL) return CST816T_ERROR_RESOURCE;
    status = handler->driver.pf_sleep(&handler->driver);

    if (status == CST816T_OK) handler->initialized = false;

    return status;
}

cst816t_status_t touch_handler_wakeup(bsp_touch_handler_t *handler)
{
    cst816t_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return CST816T_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);

    if (status == CST816T_OK) handler->initialized = true;
    
    return status;
}
