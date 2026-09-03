#include "bsp_temp_humi_handler.h"

#include <stddef.h>
#include <string.h>

temp_humi_status_t temp_humi_handler_init(
    bsp_temp_humi_handler_t *handler, temp_humi_iic_interface_t *iic,
    temp_humi_yield_interface_t *yield)
{
    temp_humi_status_t status;

    if (handler == NULL || iic == NULL || yield == NULL)
        return TEMP_HUMI_ERROR_PARAMETER;

    memset(handler, 0, sizeof(*handler));
    status = bsp_temp_humi_inst(&handler->driver, iic, yield);
    handler->initialized = status == TEMP_HUMI_OK;
    return status;
}

temp_humi_status_t temp_humi_handler_read(
    bsp_temp_humi_handler_t *handler, float *temp_c, float *humi_pct)
{
    if (handler == NULL || temp_c == NULL || humi_pct == NULL ||
        !handler->initialized || handler->driver.pf_read_temp_humi == NULL)
        return TEMP_HUMI_ERROR_RESOURCE;

    return handler->driver.pf_read_temp_humi(&handler->driver,
                                             temp_c, humi_pct);
}

temp_humi_status_t temp_humi_handler_sleep(bsp_temp_humi_handler_t *handler)
{
    temp_humi_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL)
        return TEMP_HUMI_ERROR_RESOURCE;

    status = handler->driver.pf_sleep(&handler->driver);
    if (status == TEMP_HUMI_OK) handler->initialized = false;
    return status;
}

temp_humi_status_t temp_humi_handler_wakeup(bsp_temp_humi_handler_t *handler)
{
    temp_humi_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return TEMP_HUMI_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);
    if (status == TEMP_HUMI_OK) handler->initialized = true;
    return status;
}
