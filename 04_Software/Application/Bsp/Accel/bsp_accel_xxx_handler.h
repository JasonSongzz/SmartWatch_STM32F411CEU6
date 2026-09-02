#ifndef BSP_ACCEL_HANDLER_H
#define BSP_ACCEL_HANDLER_H

#include "bsp_mpu6050_driver.h"
#include <stdbool.h>

typedef struct
{
    bsp_mpu6050_driver_t driver;
    bool initialized;
} bsp_accel_handler_t;

mpu6050_status_t accel_handler_init(bsp_accel_handler_t *handler,
                                     mpu6050_iic_driver_interface_t *iic,
                                     mpu6050_yield_interface_t *yield);
mpu6050_status_t accel_handler_read(bsp_accel_handler_t *handler,
                                     mpu6050_accel_t *accel);
mpu6050_status_t accel_handler_sleep(bsp_accel_handler_t *handler);
mpu6050_status_t accel_handler_wakeup(bsp_accel_handler_t *handler);

#endif /* BSP_ACCEL_HANDLER_H */
