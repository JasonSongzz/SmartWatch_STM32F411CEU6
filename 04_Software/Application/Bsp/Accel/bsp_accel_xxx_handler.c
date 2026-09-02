#include "bsp_accel_xxx_handler.h"

#include <stddef.h>

mpu6050_status_t accel_handler_init(bsp_accel_handler_t *handler,
                                     mpu6050_iic_driver_interface_t *iic,
                                     mpu6050_yield_interface_t *yield)
{
    mpu6050_status_t status;
    if (handler == NULL || iic == NULL || yield == NULL)
        return MPU6050_ERRORPARAMETER;
    if (handler->initialized) return MPU6050_OK;
    status = mpu6050_inst(&handler->driver, iic, yield);
    if (status == MPU6050_OK) handler->initialized = true;
    return status;
}

mpu6050_status_t accel_handler_read(bsp_accel_handler_t *handler,
                                     mpu6050_accel_t *accel)
{
    if (handler == NULL || accel == NULL || !handler->initialized ||
        handler->driver.pf_read_accel == NULL)
        return MPU6050_ERRORRESOURCE;
    return handler->driver.pf_read_accel(&handler->driver, accel);
}

mpu6050_status_t accel_handler_sleep(bsp_accel_handler_t *handler)
{
    mpu6050_status_t status;
    if (handler == NULL || !handler->initialized || handler->driver.pf_sleep == NULL)
        return MPU6050_ERRORRESOURCE;
    status = handler->driver.pf_sleep(&handler->driver);
    if (status == MPU6050_OK) handler->initialized = false;
    return status;
}

mpu6050_status_t accel_handler_wakeup(bsp_accel_handler_t *handler)
{
    mpu6050_status_t status;
    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return MPU6050_ERRORRESOURCE;
    status = handler->driver.pf_wakeup(&handler->driver);
    if (status == MPU6050_OK) handler->initialized = true;
    return status;
}
