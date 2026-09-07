#include "bsp_mpu6050_driver.h"

#include <stddef.h>

#define MPU6050_NOT_INITED 0
#define MPU6050_INITED     1
#define MPU6050_IO_TIMEOUT_MS 100U

static accel_status_t mpu6050_first_error(accel_status_t current,
                                      accel_status_t candidate)
{
    return current == ACCEL_OK ? candidate : current;
}

static accel_status_t mpu6050_lock_bus(accel_iic_interface_t *iic)
{
    if (iic->pf_lock == NULL) return ACCEL_OK;

    return iic->pf_lock(iic->bus_context, MPU6050_IO_TIMEOUT_MS);
}

static accel_status_t mpu6050_unlock_bus(accel_iic_interface_t *iic)
{
    if (iic->pf_unlock == NULL) return ACCEL_OK;

    return iic->pf_unlock(iic->bus_context);
}

static accel_status_t mpu6050_send_byte_and_wait_ack(
    accel_iic_interface_t *iic, uint8_t data)
{
    accel_status_t status;

    status = iic->pf_iic_send_byte(iic->bus_context, data);
    if (status != ACCEL_OK) return status;

    return iic->pf_iic_wait_ack(iic->bus_context);
}

static accel_status_t mpu6050_write_register(bsp_accel_driver_t *instance,
                                          uint8_t reg, uint8_t value)
{
    accel_iic_interface_t *iic = instance->p_iic_driver_instance;
    accel_status_t status;

    status = mpu6050_lock_bus(iic);
    if (status != ACCEL_OK) return status;

    void *context = iic->bus_context;
    status = iic->pf_iic_start(context);
    if (status == ACCEL_OK)
        status = mpu6050_send_byte_and_wait_ack(
            iic, (uint8_t)(MPU6050_I2C_ADDRESS << 1U));
    if (status == ACCEL_OK)
        status = mpu6050_send_byte_and_wait_ack(iic, reg);
    if (status == ACCEL_OK)
        status = mpu6050_send_byte_and_wait_ack(iic, value);

    status = mpu6050_first_error(status, iic->pf_iic_stop(context));
    return mpu6050_first_error(status, mpu6050_unlock_bus(iic));
}

static accel_status_t mpu6050_read_registers(bsp_accel_driver_t *instance,
                                          uint8_t reg, uint8_t *data,
                                          uint8_t length)
{
    accel_iic_interface_t *iic = instance->p_iic_driver_instance;
    accel_status_t status;

    status = mpu6050_lock_bus(iic);
    if (status != ACCEL_OK) return status;

    void *context = iic->bus_context;
    uint8_t index;
    status = iic->pf_iic_start(context);
    if (status == ACCEL_OK)
        status = mpu6050_send_byte_and_wait_ack(
            iic, (uint8_t)(MPU6050_I2C_ADDRESS << 1U));
    if (status == ACCEL_OK)
        status = mpu6050_send_byte_and_wait_ack(iic, reg);
    if (status == ACCEL_OK)
        status = iic->pf_iic_start(context);
    if (status == ACCEL_OK)
        status = mpu6050_send_byte_and_wait_ack(
            iic, (uint8_t)((MPU6050_I2C_ADDRESS << 1U) | 1U));

    for (index = 0U; index < length && status == ACCEL_OK; index++)
    {
        status = iic->pf_iic_receive_byte(context, &data[index]);
        if (status != ACCEL_OK) break;
        status = index + 1U < length
               ? iic->pf_iic_send_ack(context)
               : iic->pf_iic_send_no_ack(context);
    }

    status = mpu6050_first_error(status, iic->pf_iic_stop(context));
    return mpu6050_first_error(status, mpu6050_unlock_bus(iic));
}

static accel_status_t mpu6050_read_id(bsp_accel_driver_t *instance, uint8_t *id)
{
    if (instance == NULL || instance->is_inited != MPU6050_INITED || id == NULL)
        return ACCEL_ERROR_PARAMETER;
    return mpu6050_read_registers(instance, MPU6050_REG_WHO_AM_I, id, 1U);
}

static accel_status_t mpu6050_init(bsp_accel_driver_t *instance)
{
    accel_iic_interface_t *iic;
    uint8_t id;

    if (instance == NULL || instance->p_iic_driver_instance == NULL ||
        instance->p_yield_instance == NULL || instance->p_yield_instance->pf_rtos_yield == NULL)
        return ACCEL_ERROR_PARAMETER;

    iic = instance->p_iic_driver_instance;

    if (iic->pf_iic_init == NULL ||
        ((iic->pf_lock == NULL) != (iic->pf_unlock == NULL)))
        return ACCEL_ERROR_RESOURCE;

    if (iic->pf_iic_start == NULL || iic->pf_iic_stop == NULL ||
        iic->pf_iic_wait_ack == NULL || iic->pf_iic_send_byte == NULL ||
        iic->pf_iic_receive_byte == NULL || iic->pf_iic_send_ack == NULL ||
        iic->pf_iic_send_no_ack == NULL)
        return ACCEL_ERROR_RESOURCE;
    if (iic->pf_iic_init(iic->bus_context) != ACCEL_OK)
        return ACCEL_ERROR_RESOURCE;

    if (mpu6050_write_register(instance, MPU6050_REG_PWR_MGMT_1,
                         MPU6050_PWR1_DEVICE_RESET) != ACCEL_OK)
        return ACCEL_ERROR;

    instance->p_yield_instance->pf_rtos_yield(MPU6050_RESET_WAIT_MS);

    if (mpu6050_write_register(instance, MPU6050_REG_PWR_MGMT_1,
                         MPU6050_PWR1_CLKSEL_PLL_XGYRO) != ACCEL_OK ||
        mpu6050_write_register(instance, MPU6050_REG_CONFIG, 0x03U) != ACCEL_OK ||
        mpu6050_write_register(instance, MPU6050_REG_SMPLRT_DIV, 0x09U) != ACCEL_OK ||
        mpu6050_write_register(instance, MPU6050_REG_GYRO_CONFIG,
                         instance->gyro_config) != ACCEL_OK ||
        mpu6050_write_register(instance, MPU6050_REG_ACCEL_CONFIG,
                         instance->accel_config) != ACCEL_OK)
        return ACCEL_ERROR;

    if (mpu6050_read_registers(instance, MPU6050_REG_WHO_AM_I, &id, 1U) != ACCEL_OK ||
        (id & MPU6050_WHO_AM_I_MASK) != (MPU6050_I2C_ADDRESS & MPU6050_WHO_AM_I_MASK))
        return ACCEL_ERROR_ID;

    instance->is_inited = MPU6050_INITED;

    return ACCEL_OK;
}

static accel_status_t mpu6050_deinit(bsp_accel_driver_t *instance)
{
    if (instance != NULL) instance->is_inited = MPU6050_NOT_INITED;
    return ACCEL_OK;
}

static accel_status_t mpu6050_read_raw_accel(bsp_accel_driver_t *instance,
                                          accel_raw_data_t *accel)
{
    uint8_t data[MPU6050_DATA_LENGTH];

    if (instance == NULL || instance->is_inited != MPU6050_INITED || accel == NULL)
        return ACCEL_ERROR_PARAMETER;

    if (mpu6050_read_registers(instance, MPU6050_REG_ACCEL_XOUT_H, data,
                         MPU6050_DATA_LENGTH) != ACCEL_OK)
        return ACCEL_ERROR;

    accel->x = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
    accel->y = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
    accel->z = (int16_t)(((uint16_t)data[4] << 8) | data[5]);

    return ACCEL_OK;
}

static accel_status_t mpu6050_read_accel(bsp_accel_driver_t *instance,
                                      accel_data_t *accel)
{
    accel_raw_data_t raw;

    if (accel == NULL) return ACCEL_ERROR_PARAMETER;

    if (mpu6050_read_raw_accel(instance, &raw) != ACCEL_OK) return ACCEL_ERROR;

    accel->x = (float)raw.x / instance->accel_sensitivity;
    accel->y = (float)raw.y / instance->accel_sensitivity;
    accel->z = (float)raw.z / instance->accel_sensitivity;

    return ACCEL_OK;
}

static accel_status_t mpu6050_read_imu(bsp_accel_driver_t *instance,
                                         accel_imu_data_t *imu)
{
    uint8_t data[MPU6050_IMU_FRAME_LENGTH];
    accel_raw_imu_data_t raw;

    if (instance == NULL || instance->is_inited != MPU6050_INITED ||
        imu == NULL || instance->accel_sensitivity <= 0.0f ||
        instance->gyro_sensitivity <= 0.0f)
        return ACCEL_ERROR_PARAMETER;

    if (mpu6050_read_registers(instance, MPU6050_REG_ACCEL_XOUT_H, data,
                         MPU6050_IMU_FRAME_LENGTH) != ACCEL_OK)
        return ACCEL_ERROR;

    raw.accel.x = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
    raw.accel.y = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
    raw.accel.z = (int16_t)(((uint16_t)data[4] << 8) | data[5]);
    raw.temperature = (int16_t)(((uint16_t)data[6] << 8) | data[7]);
    raw.gyro.x = (int16_t)(((uint16_t)data[8] << 8) | data[9]);
    raw.gyro.y = (int16_t)(((uint16_t)data[10] << 8) | data[11]);
    raw.gyro.z = (int16_t)(((uint16_t)data[12] << 8) | data[13]);

    imu->accel_g.x = (float)raw.accel.x / instance->accel_sensitivity;
    imu->accel_g.y = (float)raw.accel.y / instance->accel_sensitivity;
    imu->accel_g.z = (float)raw.accel.z / instance->accel_sensitivity;
    imu->gyro_dps.x = (float)raw.gyro.x / instance->gyro_sensitivity;
    imu->gyro_dps.y = (float)raw.gyro.y / instance->gyro_sensitivity;
    imu->gyro_dps.z = (float)raw.gyro.z / instance->gyro_sensitivity;
    imu->temperature_c = (float)raw.temperature / 340.0f + 36.53f;

    return ACCEL_OK;
}

static accel_status_t mpu6050_sleep(bsp_accel_driver_t *instance)
{
    if (instance == NULL || instance->is_inited != MPU6050_INITED)
        return ACCEL_ERROR_RESOURCE;

    if (mpu6050_write_register(instance, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_SLEEP) != ACCEL_OK)
        return ACCEL_ERROR;

    instance->is_inited = MPU6050_NOT_INITED;

    return ACCEL_OK;
}

static accel_status_t mpu6050_wakeup(bsp_accel_driver_t *instance)
{
    if (instance == NULL || instance->p_yield_instance == NULL)
        return ACCEL_ERROR_PARAMETER;

    instance->is_inited = MPU6050_NOT_INITED;

    return mpu6050_init(instance);
}

accel_status_t mpu6050_inst(bsp_accel_driver_t *instance,
                              accel_iic_interface_t *iic,
                              accel_yield_interface_t *yield)
{
    if (instance == NULL || iic == NULL || yield == NULL || yield->pf_rtos_yield == NULL)
        return ACCEL_ERROR_PARAMETER;

    if (instance->is_inited == MPU6050_INITED) return ACCEL_ERROR_RESOURCE;

    instance->p_iic_driver_instance = iic;
    instance->p_yield_instance = yield;
    instance->is_inited = MPU6050_NOT_INITED;
    instance->accel_config = MPU6050_ACCEL_FS_4G;
    instance->gyro_config = MPU6050_GYRO_FS_500DPS;
    instance->accel_sensitivity = MPU6050_ACCEL_SENSITIVITY_4G;
    instance->gyro_sensitivity = MPU6050_GYRO_SENSITIVITY_500DPS;
    instance->pf_init = mpu6050_init;
    instance->pf_deinit = mpu6050_deinit;
    instance->pf_read_id = mpu6050_read_id;
    instance->pf_read_raw_accel = mpu6050_read_raw_accel;
    instance->pf_read_accel = mpu6050_read_accel;
    instance->pf_read_imu = mpu6050_read_imu;
    instance->pf_sleep = mpu6050_sleep;
    instance->pf_wakeup = mpu6050_wakeup;
    
    return mpu6050_init(instance);
}
