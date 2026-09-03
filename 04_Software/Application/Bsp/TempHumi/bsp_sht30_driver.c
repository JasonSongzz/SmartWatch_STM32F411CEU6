#include "bsp_sht30_driver.h"

#include <stddef.h>

/* Private compatibility names keep the protocol implementation readable. */
typedef temp_humi_status_t sht30_status_t;
typedef temp_humi_iic_interface_t sht30_iic_driver_interface_t;
typedef temp_humi_yield_interface_t sht30_yield_interface_t;
typedef bsp_temp_humi_driver_t bsp_sht30_driver_t;

#define SHT30_OK              TEMP_HUMI_OK
#define SHT30_ERROR           TEMP_HUMI_ERROR
#define SHT30_ERROR_TIMEOUT   TEMP_HUMI_ERROR_TIMEOUT
#define SHT30_ERROR_RESOURCE  TEMP_HUMI_ERROR_RESOURCE
#define SHT30_ERROR_PARAMETER TEMP_HUMI_ERROR_PARAMETER

#define SHT30_NOT_INITED       0
#define SHT30_INITED           1
#define SHT30_IO_TIMEOUT_MS    100U

static uint8_t sht30_crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = SHT30_CRC8_INITIAL;
    uint8_t index;
    uint8_t bit;

    for (index = 0U; index < length; index++) {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++) {
            crc = (crc & 0x80U) != 0U
                    ? (uint8_t)((crc << 1) ^ SHT30_CRC8_POLYNOMIAL)
                    : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static sht30_status_t sht30_first_error(sht30_status_t current,
                                         sht30_status_t candidate)
{
    return current == SHT30_OK ? candidate : current;
}

static sht30_status_t sht30_lock(sht30_iic_driver_interface_t *iic)
{
    return iic->pf_lock != NULL
             ? iic->pf_lock(iic->bus_context, SHT30_IO_TIMEOUT_MS)
             : SHT30_OK;
}

static sht30_status_t sht30_unlock(sht30_iic_driver_interface_t *iic)
{
    return iic->pf_unlock != NULL
             ? iic->pf_unlock(iic->bus_context) : SHT30_OK;
}

static sht30_status_t sht30_send_command(bsp_sht30_driver_t *instance,
                                          uint16_t command)
{
    sht30_iic_driver_interface_t *iic = instance->p_iic_driver_instance;
    void *context = iic->bus_context;
    uint8_t command_bytes[2] = {
        (uint8_t)(command >> 8),
        (uint8_t)command,
    };
    sht30_status_t status;
    uint8_t index;

    status = sht30_lock(iic);
    if (status != SHT30_OK) return status;

    status = iic->pf_iic_start(context);
    if (status == SHT30_OK)
        status = iic->pf_iic_send_byte(
            context, (uint8_t)(instance->i2c_address << 1));
    if (status == SHT30_OK) status = iic->pf_iic_wait_ack(context);

    for (index = 0U; index < sizeof(command_bytes) && status == SHT30_OK;
         index++) {
        status = iic->pf_iic_send_byte(context, command_bytes[index]);
        if (status == SHT30_OK) status = iic->pf_iic_wait_ack(context);
    }

    status = sht30_first_error(status, iic->pf_iic_stop(context));
    return sht30_first_error(status, sht30_unlock(iic));
}

static sht30_status_t sht30_read_bytes(bsp_sht30_driver_t *instance,
                                       uint8_t *data, uint8_t length)
{
    sht30_iic_driver_interface_t *iic = instance->p_iic_driver_instance;
    void *context = iic->bus_context;
    sht30_status_t status;
    uint8_t index;

    status = sht30_lock(iic);
    if (status != SHT30_OK) return status;

    status = iic->pf_iic_start(context);
    if (status == SHT30_OK)
        status = iic->pf_iic_send_byte(
            context, (uint8_t)((instance->i2c_address << 1) | 1U));
    if (status == SHT30_OK) status = iic->pf_iic_wait_ack(context);

    for (index = 0U; index < length && status == SHT30_OK; index++) {
        status = iic->pf_iic_receive_byte(context, &data[index]);
        if (status != SHT30_OK) break;
        status = index + 1U < length
               ? iic->pf_iic_send_ack(context)
               : iic->pf_iic_send_no_ack(context);
    }

    status = sht30_first_error(status, iic->pf_iic_stop(context));
    return sht30_first_error(status, sht30_unlock(iic));
}

static sht30_status_t sht30_read_status(bsp_sht30_driver_t *instance)
{
    uint8_t data[3];
    sht30_status_t status;

    status = sht30_send_command(instance, SHT30_CMD_READ_STATUS);
    if (status != SHT30_OK) return status;
    status = sht30_read_bytes(instance, data, sizeof(data));
    if (status != SHT30_OK) return status;

    return sht30_crc8(data, 2U) == data[2] ? SHT30_OK : SHT30_ERROR;
}

static sht30_status_t sht30_init(bsp_sht30_driver_t *instance)
{
    sht30_iic_driver_interface_t *iic;

    if (instance == NULL || instance->p_iic_driver_instance == NULL ||
        instance->p_yield_instance == NULL ||
        instance->p_yield_instance->pf_rtos_yield == NULL)
        return SHT30_ERROR_PARAMETER;

    iic = instance->p_iic_driver_instance;
    if (iic->pf_iic_init == NULL || iic->pf_iic_start == NULL ||
        iic->pf_iic_stop == NULL || iic->pf_iic_wait_ack == NULL ||
        iic->pf_iic_send_ack == NULL || iic->pf_iic_send_no_ack == NULL ||
        iic->pf_iic_send_byte == NULL || iic->pf_iic_receive_byte == NULL ||
        ((iic->pf_lock == NULL) != (iic->pf_unlock == NULL)))
        return SHT30_ERROR_RESOURCE;

    if (iic->pf_iic_init(iic->bus_context) != SHT30_OK)
        return SHT30_ERROR_RESOURCE;

    instance->p_yield_instance->pf_rtos_yield(SHT30_POWER_ON_WAIT_MS);
    if (sht30_send_command(instance, SHT30_CMD_SOFT_RESET) != SHT30_OK)
        return SHT30_ERROR_RESOURCE;
    instance->p_yield_instance->pf_rtos_yield(SHT30_RESET_WAIT_MS);

    if (sht30_read_status(instance) != SHT30_OK)
        return SHT30_ERROR_RESOURCE;
    (void)sht30_send_command(instance, SHT30_CMD_CLEAR_STATUS);

    instance->is_inited = SHT30_INITED;
    return SHT30_OK;
}

static sht30_status_t sht30_deinit(bsp_sht30_driver_t *instance)
{
    if (instance == NULL) return SHT30_ERROR_PARAMETER;
    instance->is_inited = SHT30_NOT_INITED;
    if (instance->p_iic_driver_instance->pf_iic_deinit != NULL)
        return instance->p_iic_driver_instance->pf_iic_deinit(
            instance->p_iic_driver_instance->bus_context);
    return SHT30_OK;
}

static sht30_status_t sht30_read_temp_humi(bsp_sht30_driver_t *instance,
                                            float *temperature,
                                            float *humidity)
{
    uint8_t data[6];
    uint16_t raw_temperature;
    uint16_t raw_humidity;
    sht30_status_t status;

    if (instance == NULL || instance->is_inited != SHT30_INITED ||
        temperature == NULL || humidity == NULL)
        return SHT30_ERROR_PARAMETER;

    status = sht30_send_command(instance,
                                SHT30_CMD_MEASURE_HIGH_REPEATABILITY);
    if (status != SHT30_OK) return status;
    instance->p_yield_instance->pf_rtos_yield(SHT30_MEASURE_WAIT_MS);

    status = sht30_read_bytes(instance, data, sizeof(data));
    if (status != SHT30_OK) return status;
    if (sht30_crc8(&data[0], 2U) != data[2] ||
        sht30_crc8(&data[3], 2U) != data[5])
        return SHT30_ERROR;

    raw_temperature = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    raw_humidity = (uint16_t)(((uint16_t)data[3] << 8) | data[4]);
    *temperature = -45.0f + 175.0f * (float)raw_temperature / 65535.0f;
    *humidity = 100.0f * (float)raw_humidity / 65535.0f;
    return SHT30_OK;
}

static sht30_status_t sht30_sleep(bsp_sht30_driver_t *instance)
{
    if (instance == NULL || instance->is_inited != SHT30_INITED)
        return SHT30_ERROR_RESOURCE;
    instance->is_inited = SHT30_NOT_INITED;
    return SHT30_OK;
}

static sht30_status_t sht30_wakeup(bsp_sht30_driver_t *instance)
{
    return sht30_init(instance);
}

sht30_status_t sht30_inst(bsp_sht30_driver_t *instance,
                          sht30_iic_driver_interface_t *iic,
                          sht30_yield_interface_t *yield)
{
    if (instance == NULL || iic == NULL || yield == NULL ||
        yield->pf_rtos_yield == NULL)
        return SHT30_ERROR_PARAMETER;

    instance->p_iic_driver_instance = iic;
    instance->p_yield_instance = yield;
    instance->i2c_address = SHT30_REG_I2C_ADDRESS_DEFAULT;
    instance->is_inited = SHT30_NOT_INITED;
    instance->pf_init = sht30_init;
    instance->pf_deinit = sht30_deinit;
    instance->pf_read_temp_humi = sht30_read_temp_humi;
    instance->pf_sleep = sht30_sleep;
    instance->pf_wakeup = sht30_wakeup;
    return sht30_init(instance);
}
