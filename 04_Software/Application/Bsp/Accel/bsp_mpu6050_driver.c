#include "bsp_mpu6050_driver.h"

#include <stddef.h>

#define MPU6050_NOT_INITED 0
#define MPU6050_INITED     1

static void __enter_bus(mpu6050_iic_driver_interface_t *iic)
{
#ifndef HARDWARE_IIC
    if (iic->pf_critical_enter != NULL) (void)iic->pf_critical_enter();
#else
    (void)iic;
#endif
}

static void __exit_bus(mpu6050_iic_driver_interface_t *iic)
{
#ifndef HARDWARE_IIC
    if (iic->pf_critical_exit != NULL) (void)iic->pf_critical_exit();
#else
    (void)iic;
#endif
}

static mpu6050_status_t __write_register(bsp_mpu6050_driver_t *instance,
                                          uint8_t reg, uint8_t value)
{
    mpu6050_iic_driver_interface_t *iic = instance->p_iic_driver_instance;
#ifdef HARDWARE_IIC
    return iic->pf_iic_write(iic->bus_context, MPU6050_I2C_ADDRESS, reg,
                             &value, 1U);
#else
    void *context = iic->bus_context;
    mpu6050_status_t status = MPU6050_OK;
    __enter_bus(iic);
    (void)iic->pf_iic_start(context);
    (void)iic->pf_iic_send_byte(context, (uint8_t)(MPU6050_I2C_ADDRESS << 1));

    if (iic->pf_iic_wait_ack(context) != MPU6050_OK) status = MPU6050_ERROR;

    if (status == MPU6050_OK)
    {
        (void)iic->pf_iic_send_byte(context, reg);
        if (iic->pf_iic_wait_ack(context) != MPU6050_OK) status = MPU6050_ERROR;
    }

    if (status == MPU6050_OK)
    {
        (void)iic->pf_iic_send_byte(context, value);
        if (iic->pf_iic_wait_ack(context) != MPU6050_OK) status = MPU6050_ERROR;
    }
    
    (void)iic->pf_iic_stop(context);
    __exit_bus(iic);

    return status; 
#endif
}

static mpu6050_status_t __read_registers(bsp_mpu6050_driver_t *instance,
                                          uint8_t reg, uint8_t *data,
                                          uint8_t length)
{
    mpu6050_iic_driver_interface_t *iic = instance->p_iic_driver_instance;
#ifdef HARDWARE_IIC
    return iic->pf_iic_read(iic->bus_context, MPU6050_I2C_ADDRESS, reg,
                            data, length);
#else
    void *context = iic->bus_context;
    uint8_t index;
    mpu6050_status_t status = MPU6050_OK;
    __enter_bus(iic);
    (void)iic->pf_iic_start(context);
    (void)iic->pf_iic_send_byte(context, (uint8_t)(MPU6050_I2C_ADDRESS << 1));

    if (iic->pf_iic_wait_ack(context) != MPU6050_OK) status = MPU6050_ERROR;

    if (status == MPU6050_OK)
    {
        (void)iic->pf_iic_send_byte(context, reg);
        if (iic->pf_iic_wait_ack(context) != MPU6050_OK) status = MPU6050_ERROR;
    }

    if (status == MPU6050_OK)
    {
        (void)iic->pf_iic_start(context);
        (void)iic->pf_iic_send_byte(context, (uint8_t)((MPU6050_I2C_ADDRESS << 1) | 1U));
        if (iic->pf_iic_wait_ack(context) != MPU6050_OK) status = MPU6050_ERROR;
    }

    for (index = 0U; index < length && status == MPU6050_OK; index++)
    {
        (void)iic->pf_iic_receive_byte(context, &data[index]);
        if (index + 1U < length) (void)iic->pf_iic_send_ack(context);
        else (void)iic->pf_iic_send_no_ack(context);
    }

    (void)iic->pf_iic_stop(context);
    __exit_bus(iic);

    return status;
#endif
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

    if (iic->pf_iic_init == NULL)
        return MPU6050_ERRORRESOURCE;

#ifndef HARDWARE_IIC
    if (iic->pf_iic_start == NULL || iic->pf_iic_stop == NULL ||
        iic->pf_iic_wait_ack == NULL || iic->pf_iic_send_byte == NULL ||
        iic->pf_iic_receive_byte == NULL || iic->pf_iic_send_ack == NULL ||
        iic->pf_iic_send_no_ack == NULL)
        return MPU6050_ERRORRESOURCE;
#else
    if (iic->pf_iic_write == NULL || iic->pf_iic_read == NULL)
        return MPU6050_ERRORRESOURCE;
#endif
    (void)iic->pf_iic_init(iic->bus_context);

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
    instance->pf_init = (mpu6050_status_t (*)(void * const))mpu6050_init;
    instance->pf_deinit = (mpu6050_status_t (*)(void * const))mpu6050_deinit;
    instance->pf_read_id = (mpu6050_status_t (*)(void * const, uint8_t * const))mpu6050_read_id;
    instance->pf_read_raw_accel = (mpu6050_status_t (*)(void * const, mpu6050_raw_accel_t * const))mpu6050_read_raw_accel;
    instance->pf_read_accel = (mpu6050_status_t (*)(void * const, mpu6050_accel_t * const))mpu6050_read_accel;
    instance->pf_sleep = (mpu6050_status_t (*)(void * const))mpu6050_sleep;
    instance->pf_wakeup = (mpu6050_status_t (*)(void * const))mpu6050_wakeup;
    
    return mpu6050_init(instance);
}
