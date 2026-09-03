#ifndef BSP_TEMP_HUMI_HANDLER_H
#define BSP_TEMP_HUMI_HANDLER_H

/* Change only this include when replacing the temperature/humidity sensor. */
#include "bsp_aht21_driver.h"

#include <stdbool.h>

typedef struct
{
    bsp_temp_humi_driver_t driver;
    bool initialized;
} bsp_temp_humi_handler_t;

temp_humi_status_t temp_humi_handler_init(
    bsp_temp_humi_handler_t *handler, temp_humi_iic_interface_t *iic,
    temp_humi_yield_interface_t *yield);
temp_humi_status_t temp_humi_handler_read(
    bsp_temp_humi_handler_t *handler, float *temp_c, float *humi_pct);
temp_humi_status_t temp_humi_handler_sleep(bsp_temp_humi_handler_t *handler);
temp_humi_status_t temp_humi_handler_wakeup(bsp_temp_humi_handler_t *handler);

#endif /* BSP_TEMP_HUMI_HANDLER_H */
