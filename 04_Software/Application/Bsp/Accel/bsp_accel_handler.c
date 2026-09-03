#include "bsp_accel_handler.h"

#include <stddef.h>
#include <string.h>

accel_status_t accel_handler_init(bsp_accel_handler_t *handler,
                                  accel_iic_interface_t *iic,
                                  accel_yield_interface_t *yield)
{
    accel_status_t status;

    if (handler == NULL || iic == NULL || yield == NULL)
        return ACCEL_ERROR_PARAMETER;

    memset(handler, 0, sizeof(*handler));
    status = bsp_accel_inst(&handler->driver, iic, yield);
    handler->initialized = status == ACCEL_OK;
    return status;
}

accel_status_t accel_handler_read(bsp_accel_handler_t *handler,
                                  accel_data_t *accel)
{
    if (handler == NULL || accel == NULL || !handler->initialized ||
        handler->driver.pf_read_accel == NULL)
        return ACCEL_ERROR_RESOURCE;

    return handler->driver.pf_read_accel(&handler->driver, accel);
}

accel_status_t accel_handler_sleep(bsp_accel_handler_t *handler)
{
    accel_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL)
        return ACCEL_ERROR_RESOURCE;

    status = handler->driver.pf_sleep(&handler->driver);
    if (status == ACCEL_OK) handler->initialized = false;
    return status;
}

accel_status_t accel_handler_wakeup(bsp_accel_handler_t *handler)
{
    accel_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return ACCEL_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);
    if (status == ACCEL_OK) handler->initialized = true;
    return status;
}
