#include "bsp_aht21_driver.h"

#include <stddef.h>

#define AHT21_NOT_INITED 0
#define AHT21_INITED     1
#define AHT21_IO_TIMEOUT_MS     100U
#define AHT21_CRC8_POLYNOMIAL 0x31U
#define AHT21_CRC8_INITIAL    0xFFU

static uint8_t aht21_crc8(const uint8_t *data, uint8_t length)
{
	uint8_t crc = AHT21_CRC8_INITIAL;
	uint8_t index;
	uint8_t bit;

	for (index = 0U; index < length; index++)
	{
		crc ^= data[index];
		for (bit = 0U; bit < 8U; bit++)
		{
			crc = (crc & 0x80U) != 0U
				? (uint8_t)((crc << 1) ^ AHT21_CRC8_POLYNOMIAL)
				: (uint8_t)(crc << 1);
		}
	}

	return crc;
}

static temp_humi_status_t aht21_first_error(temp_humi_status_t current,
									 temp_humi_status_t candidate)
{
	return current == TEMP_HUMI_OK ? candidate : current;
}

static temp_humi_status_t aht21_lock_bus(temp_humi_iic_interface_t *iic)
{
	if (iic->pf_lock == NULL)
	{
		return TEMP_HUMI_OK;
	}

	return iic->pf_lock(iic->bus_context, AHT21_IO_TIMEOUT_MS);
}

static temp_humi_status_t aht21_unlock_bus(temp_humi_iic_interface_t *iic)
{
	if (iic->pf_unlock == NULL)
	{
		return TEMP_HUMI_OK;
	}

	return iic->pf_unlock(iic->bus_context);
}

static temp_humi_status_t aht21_send_command(bsp_temp_humi_driver_t *instance,
										  uint8_t command,
										  uint8_t parameter_1,
										  uint8_t parameter_2)
{
	temp_humi_iic_interface_t *iic = instance->p_iic_driver_instance;
	uint8_t bytes[3] = { command, parameter_1, parameter_2 };
	temp_humi_status_t status;

	status = aht21_lock_bus(iic);
	if (status != TEMP_HUMI_OK)
	{
		return status;
	}

	void *context = iic->bus_context;
	uint8_t index;

	status = iic->pf_iic_start(context);
	if (status == TEMP_HUMI_OK)
	{
		status = iic->pf_iic_send_byte(
			context, (uint8_t)(AHT21_REG_I2C_ADDRESS << 1));
	}

	if (status == TEMP_HUMI_OK)
	{
		status = iic->pf_iic_wait_ack(context);
	}

	for (index = 0U; index < 3U && status == TEMP_HUMI_OK; index++)
	{
		status = iic->pf_iic_send_byte(context, bytes[index]);
		if (status == TEMP_HUMI_OK)
		{
			status = iic->pf_iic_wait_ack(context);
		}
	}

	status = aht21_first_error(status, iic->pf_iic_stop(context));
	return aht21_first_error(status, aht21_unlock_bus(iic));
}

static temp_humi_status_t aht21_read_bytes(bsp_temp_humi_driver_t *instance,
										uint8_t *data,
										uint8_t length)
{
	temp_humi_iic_interface_t *iic = instance->p_iic_driver_instance;
	temp_humi_status_t status;

	status = aht21_lock_bus(iic);
	if (status != TEMP_HUMI_OK)
	{
		return status;
	}

	void *context = iic->bus_context;
	uint8_t index;

	status = iic->pf_iic_start(context);
	if (status == TEMP_HUMI_OK)
	{
		status = iic->pf_iic_send_byte(
			context,
			(uint8_t)((AHT21_REG_I2C_ADDRESS << 1) | 1U));
	}

	if (status == TEMP_HUMI_OK)
	{
		status = iic->pf_iic_wait_ack(context);
	}

	for (index = 0U; index < length && status == TEMP_HUMI_OK; index++)
	{
		status = iic->pf_iic_receive_byte(context, &data[index]);
		if (status != TEMP_HUMI_OK)
		{
			break;
		}

		if (index + 1U < length)
		{
			status = iic->pf_iic_send_ack(context);
		}
		else
		{
			status = iic->pf_iic_send_no_ack(context);
		}
	}

	status = aht21_first_error(status, iic->pf_iic_stop(context));
	return aht21_first_error(status, aht21_unlock_bus(iic));
}

static temp_humi_status_t aht21_read_status(bsp_temp_humi_driver_t *instance, uint8_t *status)
{
	return aht21_read_bytes(instance, status, 1U);
}

static temp_humi_status_t aht21_init(bsp_temp_humi_driver_t *instance)
{
	temp_humi_iic_interface_t *iic;
	uint8_t status;

	if (instance == NULL || instance->p_iic_driver_instance == NULL ||
		instance->p_yield_instance == NULL ||
		instance->p_yield_instance->pf_rtos_yield == NULL)
	{
		return TEMP_HUMI_ERROR_PARAMETER;
	}

	iic = instance->p_iic_driver_instance;

	if (iic->pf_iic_init == NULL ||
		((iic->pf_lock == NULL) != (iic->pf_unlock == NULL)) ||
		iic->pf_iic_start == NULL ||
		iic->pf_iic_stop == NULL || iic->pf_iic_send_byte == NULL ||
		iic->pf_iic_wait_ack == NULL || iic->pf_iic_receive_byte == NULL ||
		iic->pf_iic_send_ack == NULL || iic->pf_iic_send_no_ack == NULL)
	{
		return TEMP_HUMI_ERROR_RESOURCE;
	}

	if (iic->pf_iic_init(iic->bus_context) != TEMP_HUMI_OK)
	{
		return TEMP_HUMI_ERROR_RESOURCE;
	}

	instance->p_yield_instance->pf_rtos_yield(AHT21_POWER_ON_WAIT_MS);

	if (aht21_read_status(instance, &status) != TEMP_HUMI_OK)
	{
		return TEMP_HUMI_ERROR_RESOURCE;
	}

	if ((status & AHT21_STATUS_CALIBRATED) == 0U &&
		aht21_send_command(instance, AHT21_REG_INITIALIZE,
						   AHT21_REG_INITIALIZE_PARAM, 0U) != TEMP_HUMI_OK)
	{
		return TEMP_HUMI_ERROR;
	}

	if ((status & AHT21_STATUS_CALIBRATED) == 0U)
	{
		instance->p_yield_instance->pf_rtos_yield(AHT21_INIT_WAIT_MS);
	}
    
	instance->is_inited = AHT21_INITED;

	return TEMP_HUMI_OK;
}

static temp_humi_status_t aht21_deinit(bsp_temp_humi_driver_t *instance)
{
	if (instance == NULL) return TEMP_HUMI_ERROR_PARAMETER;

	instance->is_inited = AHT21_NOT_INITED;
	if (instance->p_iic_driver_instance != NULL &&
		instance->p_iic_driver_instance->pf_iic_deinit != NULL)
		return instance->p_iic_driver_instance->pf_iic_deinit(
			instance->p_iic_driver_instance->bus_context);

	return TEMP_HUMI_OK;
}

static temp_humi_status_t aht21_read_id(bsp_temp_humi_driver_t *instance)
{
	return (instance != NULL && instance->is_inited == AHT21_INITED)
		? TEMP_HUMI_OK : TEMP_HUMI_ERROR_RESOURCE;
}

static temp_humi_status_t aht21_read_temp_humi(bsp_temp_humi_driver_t *instance,
											float *temperature,
											float *humidity)
{
	uint8_t data[7];
	uint32_t raw_humidity;
	uint32_t raw_temperature;
	temp_humi_status_t status;

	if (instance == NULL || instance->is_inited != AHT21_INITED ||
		temperature == NULL || humidity == NULL)
	{
		return TEMP_HUMI_ERROR_PARAMETER;
	}

	status = aht21_send_command(instance, AHT21_REG_TRIGGER_MEASURE,
							 AHT21_REG_MEASURE_PARAM_1,
							 AHT21_REG_MEASURE_PARAM_2);
	if (status != TEMP_HUMI_OK) return status;

	instance->p_yield_instance->pf_rtos_yield(AHT21_MEASURE_WAIT_MS);

	status = aht21_read_bytes(instance, data, (uint8_t)sizeof(data));
	if (status != TEMP_HUMI_OK) return status;
	if ((data[0] & AHT21_STATUS_BUSY) != 0U)
		return TEMP_HUMI_ERROR_TIMEOUT;
	if (aht21_crc8(data, 6U) != data[6]) return TEMP_HUMI_ERROR;

	raw_humidity = ((uint32_t)data[1] << 12) |
				   ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);
	raw_temperature = (((uint32_t)data[3] & 0x0FU) << 16) |
					  ((uint32_t)data[4] << 8) | data[5];
	*humidity = (float)raw_humidity * 100.0f / 1048576.0f;
	*temperature = (float)raw_temperature * 200.0f / 1048576.0f - 50.0f;

	return TEMP_HUMI_OK;
}

static temp_humi_status_t aht21_sleep(bsp_temp_humi_driver_t *instance)
{
	return aht21_read_id(instance);
}

static temp_humi_status_t aht21_wakeup(bsp_temp_humi_driver_t *instance)
{
	return aht21_read_id(instance);
}

temp_humi_status_t aht21_inst(bsp_temp_humi_driver_t *instance,
						  temp_humi_iic_interface_t *iic,
						  temp_humi_yield_interface_t *yield)
{
	if (instance == NULL || iic == NULL || yield == NULL ||
		yield->pf_rtos_yield == NULL)
	{
		return TEMP_HUMI_ERROR_PARAMETER;
	}

	if (instance->is_inited == AHT21_INITED)
	{
		return TEMP_HUMI_ERROR_RESOURCE;
	}

	instance->is_inited = AHT21_NOT_INITED;
	instance->p_iic_driver_instance = iic;
	instance->p_yield_instance = yield;
	instance->i2c_address = AHT21_REG_I2C_ADDRESS;
	instance->pf_init = aht21_init;
	instance->pf_deinit = aht21_deinit;
	instance->pf_read_temp_humi = aht21_read_temp_humi;
	instance->pf_sleep = aht21_sleep;
	instance->pf_wakeup = aht21_wakeup;

	return aht21_init(instance);
}
