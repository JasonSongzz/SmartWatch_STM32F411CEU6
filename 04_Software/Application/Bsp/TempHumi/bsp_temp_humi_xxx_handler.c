#include "bsp_temp_humi_xxx_handler.h"

sht30_status_t temp_humi_handler_init(bsp_temp_humi_sht30_handler_t *handler,
                                      iic_driver_interface_t *iic,
                                      yield_interface_t *yield,
                                      timebase_interface_t *timebase)
{
    sht30_status_t ret;
    if (handler == NULL || iic == NULL || yield == NULL || timebase == NULL) {
        return SHT30_ERRORPARAMETER;
    }
    if (handler->initialized) return SHT30_OK;

    ret = sht30_inst(&handler->driver, iic, yield, timebase);
    if (ret == SHT30_OK) handler->initialized = true;
    return ret;
}

sht30_status_t temp_humi_handler_read(bsp_temp_humi_sht30_handler_t *handler,
                                      float *temp_c, float *humi_pct)
{
    if (handler == NULL || temp_c == NULL || humi_pct == NULL ||
        !handler->initialized || handler->driver.pf_read_temp_humi == NULL) {
        return SHT30_ERRORRESOURCE;
    }
    return handler->driver.pf_read_temp_humi(&handler->driver, temp_c, humi_pct);
}
