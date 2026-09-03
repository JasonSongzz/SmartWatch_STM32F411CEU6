#ifndef BSP_ACCEL_HANDLER_H
#define BSP_ACCEL_HANDLER_H

/* Change only this include when replacing the accelerometer. */
#include "bsp_mpu6050_driver.h"

#include <stdbool.h>

typedef struct
{
    bsp_accel_driver_t driver;
    bool initialized;
} bsp_accel_handler_t;

accel_status_t accel_handler_init(bsp_accel_handler_t *handler,
                                  accel_iic_interface_t *iic,
                                  accel_yield_interface_t *yield);
accel_status_t accel_handler_read(bsp_accel_handler_t *handler,
                                  accel_data_t *accel);
accel_status_t accel_handler_sleep(bsp_accel_handler_t *handler);
accel_status_t accel_handler_wakeup(bsp_accel_handler_t *handler);

#endif /* BSP_ACCEL_HANDLER_H */
