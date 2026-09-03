#include "bsp_mpu6050_driver.h"

#include <stddef.h>

typedef accel_status_t mpu6050_status_t;
typedef accel_iic_interface_t mpu6050_iic_driver_interface_t;
typedef accel_yield_interface_t mpu6050_yield_interface_t;
typedef accel_raw_data_t mpu6050_raw_accel_t;
typedef accel_data_t mpu6050_accel_t;
typedef bsp_accel_driver_t bsp_mpu6050_driver_t;

#define MPU6050_OK             ACCEL_OK
#define MPU6050_ERROR          ACCEL_ERROR
#define MPU6050_ERRORTIMEOUT   ACCEL_ERROR_TIMEOUT
#define MPU6050_ERRORRESOURCE  ACCEL_ERROR_RESOURCE
#define MPU6050_ERRORPARAMETER ACCEL_ERROR_PARAMETER
#define MPU6050_ERRORID        ACCEL_ERROR_ID

#define MPU6050_NOT_INITED 0
#define MPU6050_INITED     1
#define MPU6050_IO_TIMEOUT_MS 100U

static mpu6050_status_t __first_error(mpu6050_status_t current,
                                      mpu6050_status_t candidate)
{
    return current == MPU6050_OK ? candidate : current;
}

static mpu6050_status_t __lock_bus(mpu6050_iic_driver_interface_t *iic)
{
    if (iic->pf_lock == NULL) return MPU6050_OK;

    return iic->pf_lock(iic->bus_context, MPU6050_IO_TIMEOUT_MS);
}

static mpu6050_status_t __unlock_bus(mpu6050_iic_driver_interface_t *iic)
{
    if (iic->pf_unlock == NULL) return MPU6050_OK;

    return iic->pf_unlock(iic->bus_context);
}

static mpu6050_status_t __send_byte_and_wait_ack(
    mpu6050_iic_driver_interface_t *iic, uint8_t data)
{
    mpu6050_status_t status;

    status = iic->pf_iic_send_byte(iic->bus_context, data);
    if (status != MPU6050_OK) return status;

    return iic->pf_iic_wait_ack(iic->bus_context);
}

static mpu6050_status_t __write_register(bsp_mpu6050_driver_t *instance,
                                          uint8_t reg, uint8_t value)
{
    mpu6050_iic_driver_interface_t *iic = instance->p_iic_driver_instance;
    mpu6050_status_t status;

    status = __lock_bus(iic);
    if (status != MPU6050_OK) return status;

    void *context = iic->bus_context;
    status = iic->pf_iic_start(context);
    if (status == MPU6050_OK)
        status = __send_byte_and_wait_ack(
            iic, (uint8_t)(MPU6050_I2C_ADDRESS << 1U));
    if (status == MPU6050_OK)
        status = __send_byte_and_wait_ack(iic, reg);
    if (status == MPU6050_OK)
        status = __send_byte_and_wait_ack(iic, value);

    status = __first_error(status, iic->pf_iic_stop(context));
    return __first_error(status, __unlock_bus(iic));
}

static mpu6050_status_t __read_registers(bsp_mpu6050_driver_t *instance,
                                          uint8_t reg, uint8_t *data,
                                          uint8_t length)
{
    mpu6050_iic_driver_interface_t *iic = instance->p_iic_driver_instance;
    mpu6050_status_t status;

    status = __lock_bus(iic);
    if (status != MPU6050_OK) return status;

    void *context = iic->bus_context;
    uint8_t index;
    status = iic->pf_iic_start(context);
    if (status == MPU6050_OK)
        status = __send_byte_and_wait_ack(
            iic, (uint8_t)(MPU6050_I2C_ADDRESS << 1U));
    if (status == MPU6050_OK)
        status = __send_byte_and_wait_ack(iic, reg);
    if (status == MPU6050_OK)
        status = iic->pf_iic_start(context);
    if (status == MPU6050_OK)
        status = __send_byte_and_wait_ack(
            iic, (uint8_t)((MPU6050_I2C_ADDRESS << 1U) | 1U));

    for (index = 0U; index < length && status == MPU6050_OK; index++)
    {
        status = iic->pf_iic_receive_byte(context, &data[index]);
        if (status != MPU6050_OK) break;
        status = index + 1U < length
               ? iic->pf_iic_send_ack(context)
               : iic->pf_iic_send_no_ack(context);
    }

    status = __first_error(status, iic->pf_iic_stop(context));
    return __first_error(status, __unlock_bus(iic));
}

static mpu6050_status_t mpu6050_read_id(bsp_mpu6050_driver_t *instance, uint8_t *id)
{
    if (instance == NULL || instance->is_inited != MPU6050_INITED || id == NULL)
        return MPU6050_ERRORPARAMETER;
    return __read_registers(instance, MPU6050_REG_WHO_AM_I, id, 1U);
}

static mpu6050_status_t mpu6050_init(bsp_mpu6050_driver_t *instance)
{
    mpu6050_iic_driver_interface_t *iic;
    uint8_t id;

    if (instance == NULL || instance->p_iic_driver_instance == NULL ||
        instance->p_yield_instance == NULL || instance->p_yield_instance->pf_rtos_yield == NULL)
        return MPU6050_ERRORPARAMETER;

    iic = instance->p_iic_driver_instance;

    if (iic->pf_iic_init == NULL ||
        ((iic->pf_lock == NULL) != (iic->pf_unlock == NULL)))
        return MPU6050_ERRORRESOURCE;

    if (iic->pf_iic_start == NULL || iic->pf_iic_stop == NULL ||
        iic->pf_iic_wait_ack == NULL || iic->pf_iic_send_byte == NULL ||
        iic->pf_iic_receive_byte == NULL || iic->pf_iic_send_ack == NULL ||
        iic->pf_iic_send_no_ack == NULL)
        return MPU6050_ERRORRESOURCE;
    if (iic->pf_iic_init(iic->bus_context) != MPU6050_OK)
        return MPU6050_ERRORRESOURCE;

    if (__write_register(instance, MPU6050_REG_PWR_MGMT_1,
                         MPU6050_PWR1_DEVICE_RESET) != MPU6050_OK)
        return MPU6050_ERROR;

    instance->p_yield_instance->pf_rtos_yield(MPU6050_RESET_WAIT_MS);

    if (__write_register(instance, MPU6050_REG_PWR_MGMT_1,
                         MPU6050_PWR1_CLKSEL_PLL_XGYRO) != MPU6050_OK ||
        __write_register(instance, MPU6050_REG_CONFIG, 0x03U) != MPU6050_OK ||
        __write_register(instance, MPU6050_REG_SMPLRT_DIV, 0x09U) != MPU6050_OK ||
        __write_register(instance, MPU6050_REG_GYRO_CONFIG, 0x00U) != MPU6050_OK ||
        __write_register(instance, MPU6050_REG_ACCEL_CONFIG,
                         instance->accel_config) != MPU6050_OK)
        return MPU6050_ERROR;

    if (__read_registers(instance, MPU6050_REG_WHO_AM_I, &id, 1U) != MPU6050_OK ||
        (id & MPU6050_WHO_AM_I_MASK) != (MPU6050_I2C_ADDRESS & MPU6050_WHO_AM_I_MASK))
        return MPU6050_ERRORID;

    instance->is_inited = MPU6050_INITED;

    return MPU6050_OK;
}

static mpu6050_status_t mpu6050_deinit(bsp_mpu6050_driver_t *instance)
{
    if (instance != NULL) instance->is_inited = MPU6050_NOT_INITED;
    return MPU6050_OK;
}

static mpu6050_status_t mpu6050_read_raw_accel(bsp_mpu6050_driver_t *instance,
                                          mpu6050_raw_accel_t *accel)
{
    uint8_t data[MPU6050_DATA_LENGTH];

    if (instance == NULL || instance->is_inited != MPU6050_INITED || accel == NULL)
        return MPU6050_ERRORPARAMETER;

    if (__read_registers(instance, MPU6050_REG_ACCEL_XOUT_H, data,
                         MPU6050_DATA_LENGTH) != MPU6050_OK)
        return MPU6050_ERROR;

    accel->x = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
    accel->y = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
    accel->z = (int16_t)(((uint16_t)data[4] << 8) | data[5]);

    return MPU6050_OK;
}

static mpu6050_status_t mpu6050_read_accel(bsp_mpu6050_driver_t *instance,
                                      mpu6050_accel_t *accel)
{
    mpu6050_raw_accel_t raw;

    if (accel == NULL) return MPU6050_ERRORPARAMETER;

    if (mpu6050_read_raw_accel(instance, &raw) != MPU6050_OK) return MPU6050_ERROR;

    accel->x = (float)raw.x / instance->accel_sensitivity;
    accel->y = (float)raw.y / instance->accel_sensitivity;
    accel->z = (float)raw.z / instance->accel_sensitivity;

    return MPU6050_OK;
}

static mpu6050_status_t mpu6050_sleep(bsp_mpu6050_driver_t *instance)
{
    if (instance == NULL || instance->is_inited != MPU6050_INITED)
        return MPU6050_ERRORRESOURCE;

    if (__write_register(instance, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_SLEEP) != MPU6050_OK)
        return MPU6050_ERROR;

    instance->is_inited = MPU6050_NOT_INITED;

    return MPU6050_OK;
}

static mpu6050_status_t mpu6050_wakeup(bsp_mpu6050_driver_t *instance)
{
    if (instance == NULL || instance->p_yield_instance == NULL)
        return MPU6050_ERRORPARAMETER;

    instance->is_inited = MPU6050_NOT_INITED;

    return mpu6050_init(instance);
}

mpu6050_status_t mpu6050_inst(bsp_mpu6050_driver_t *instance,
                              mpu6050_iic_driver_interface_t *iic,
                              mpu6050_yield_interface_t *yield)
{
    if (instance == NULL || iic == NULL || yield == NULL || yield->pf_rtos_yield == NULL)
        return MPU6050_ERRORPARAMETER;

    if (instance->is_inited == MPU6050_INITED) return MPU6050_ERRORRESOURCE;

    instance->p_iic_driver_instance = iic;
    instance->p_yield_instance = yield;
    instance->is_inited = MPU6050_NOT_INITED;
    instance->accel_config = MPU6050_ACCEL_FS_2G;
    instance->accel_sensitivity = MPU6050_ACCEL_SENSITIVITY_2G;
    instance->pf_init = mpu6050_init;
    instance->pf_deinit = mpu6050_deinit;
    instance->pf_read_id = mpu6050_read_id;
    instance->pf_read_raw_accel = mpu6050_read_raw_accel;
    instance->pf_read_accel = mpu6050_read_accel;
    instance->pf_sleep = mpu6050_sleep;
    instance->pf_wakeup = mpu6050_wakeup;
    
    return mpu6050_init(instance);
}
