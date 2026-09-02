#ifndef BSP_TEMP_HUMI_SHT30_HANDLER_H
#define BSP_TEMP_HUMI_SHT30_HANDLER_H

#include "bsp_sht30_driver.h"
#include <stdbool.h>

/* Handler 是无任务的设备对象：管理 SHT30 driver 生命周期，不拥有 RTOS Task。 */
typedef struct {
    bsp_sht30_driver_t driver;
    bool initialized;
} bsp_temp_humi_sht30_handler_t;

sht30_status_t temp_humi_handler_init(bsp_temp_humi_sht30_handler_t *handler,
                                      iic_driver_interface_t *iic,
                                      yield_interface_t *yield,
                                      timebase_interface_t *timebase);
sht30_status_t temp_humi_handler_read(bsp_temp_humi_sht30_handler_t *handler,
                                      float *temp_c, float *humi_pct);

#endif /* BSP_TEMP_HUMI_SHT30_HANDLER_H */
