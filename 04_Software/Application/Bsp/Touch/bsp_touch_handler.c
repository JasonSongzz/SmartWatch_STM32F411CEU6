#include "bsp_touch_handler.h"

#include <stddef.h>
#include <string.h>

touch_status_t touch_handler_init(
    bsp_touch_handler_t *handler, const touch_iic_interface_t *i2c,
    const touch_control_interface_t *control,
    const touch_yield_interface_t *yield)
{
    touch_status_t status;

    if (handler == NULL || i2c == NULL || control == NULL || yield == NULL)
        return TOUCH_ERROR_PARAMETER;

    memset(handler, 0, sizeof(*handler));
    status = bsp_touch_inst(&handler->driver, i2c, yield, control);
    handler->initialized = status == TOUCH_OK;
    return status;
}

touch_status_t touch_handler_get_info(const bsp_touch_handler_t *handler,
                                      touch_info_t *info)
{
    if (handler == NULL || info == NULL || !handler->initialized ||
        handler->driver.pf_get_info == NULL)
        return TOUCH_ERROR_RESOURCE;

    return handler->driver.pf_get_info(&handler->driver, info);
}

touch_status_t touch_handler_read(bsp_touch_handler_t *handler,
                                  touch_point_t *point)
{
    if (handler == NULL || !handler->initialized || point == NULL ||
        handler->driver.pf_read_point == NULL)
        return TOUCH_ERROR_RESOURCE;

    return handler->driver.pf_read_point(&handler->driver, point);
}

touch_status_t touch_handler_sleep(bsp_touch_handler_t *handler)
{
    touch_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL)
        return TOUCH_ERROR_RESOURCE;

    status = handler->driver.pf_sleep(&handler->driver);
    if (status == TOUCH_OK) handler->initialized = false;
    return status;
}

touch_status_t touch_handler_wakeup(bsp_touch_handler_t *handler)
{
    touch_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return TOUCH_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);
    if (status == TOUCH_OK) handler->initialized = true;
    return status;
}
