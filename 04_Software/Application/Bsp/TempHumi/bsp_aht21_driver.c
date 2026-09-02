#include "bsp_aht21_driver.h"

#include <stddef.h>

#define AHT21_NOT_INITED 0
#define AHT21_INITED     1
#define AHT21_CRC8_POLYNOMIAL 0x31U
#define AHT21_CRC8_INITIAL    0xFFU

static uint8_t __crc8(const uint8_t *data, uint8_t length)
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

static void __enter_bus(aht21_iic_driver_interface_t *iic)
{
#ifndef HARDWARE_IIC
	if (iic->pf_critical_enter != NULL)
	{
		(void)iic->pf_critical_enter();
	}
#else
	(void)iic;
#endif /* HARDWARE_IIC */
}

static void __exit_bus(aht21_iic_driver_interface_t *iic)
{
#ifndef HARDWARE_IIC
	if (iic->pf_critical_exit != NULL)
	{
		(void)iic->pf_critical_exit();
	}
#else
	(void)iic;
#endif /* HARDWARE_IIC */
}

static aht21_status_t __send_command(bsp_aht21_driver_t *instance,
										  uint8_t command,
										  uint8_t parameter_1,
										  uint8_t parameter_2)
{
	aht21_iic_driver_interface_t *iic = instance->p_iic_driver_instance;
	uint8_t bytes[3] = { command, parameter_1, parameter_2 };

#ifdef HARDWARE_IIC
	return iic->pf_iic_write(iic->bus_context, AHT21_REG_I2C_ADDRESS,
							 bytes, sizeof(bytes));
#else
	void *context = iic->bus_context;
	uint8_t index;
	aht21_status_t status = AHT21_OK;

	__enter_bus(iic);
	(void)iic->pf_iic_start(context);
	(void)iic->pf_iic_send_byte(context, (uint8_t)(AHT21_REG_I2C_ADDRESS << 1));

	if (iic->pf_iic_wait_ack(context) != AHT21_OK)
	{
		status = AHT21_ERROR;
	}

	for (index = 0U; index < 3U && status == AHT21_OK; index++)
	{
		(void)iic->pf_iic_send_byte(context, bytes[index]);
		if (iic->pf_iic_wait_ack(context) != AHT21_OK)
		{
			status = AHT21_ERROR;
		}
	}
    
	(void)iic->pf_iic_stop(context);
	__exit_bus(iic);

	return status;
#endif /* HARDWARE_IIC */
}

static aht21_status_t __read_bytes(bsp_aht21_driver_t *instance,
										uint8_t *data,
										uint8_t length)
{
	aht21_iic_driver_interface_t *iic = instance->p_iic_driver_instance;

#ifdef HARDWARE_IIC
	return iic->pf_iic_read(iic->bus_context, AHT21_REG_I2C_ADDRESS,
							data, length);
#else
	void *context = iic->bus_context;
	uint8_t index;
	aht21_status_t status = AHT21_OK;

	__enter_bus(iic);
	(void)iic->pf_iic_start(context);
	(void)iic->pf_iic_send_byte(context, (uint8_t)((AHT21_REG_I2C_ADDRESS << 1) | 1U));

	if (iic->pf_iic_wait_ack(context) != AHT21_OK)
	{
		status = AHT21_ERROR;
	}

	for (index = 0U; index < length && status == AHT21_OK; index++)
	{
		(void)iic->pf_iic_receive_byte(context, &data[index]);
		if (index + 1U < length)
		{
			(void)iic->pf_iic_send_ack(context);
		}
		else
		{
			(void)iic->pf_iic_send_no_ack(context);
		}
	}

	(void)iic->pf_iic_stop(context);
	__exit_bus(iic);

	return status;
#endif /* HARDWARE_IIC */
}

static aht21_status_t __read_status(bsp_aht21_driver_t *instance, uint8_t *status)
{
	return __read_bytes(instance, status, 1U);
}

static aht21_status_t aht21_init(bsp_aht21_driver_t *instance)
{
	aht21_iic_driver_interface_t *iic;
	uint8_t status;

	if (instance == NULL || instance->p_iic_driver_instance == NULL ||
		instance->p_yield_instance == NULL)
	{
		return AHT21_ERRORPARAMETER;
	}

	iic = instance->p_iic_driver_instance;

	if (iic->pf_iic_init == NULL
#ifndef HARDWARE_IIC
		|| iic->pf_iic_start == NULL ||
		iic->pf_iic_stop == NULL || iic->pf_iic_send_byte == NULL ||
		iic->pf_iic_wait_ack == NULL || iic->pf_iic_receive_byte == NULL ||
		iic->pf_iic_send_ack == NULL || iic->pf_iic_send_no_ack == NULL)
#else
		|| iic->pf_iic_write == NULL || iic->pf_iic_read == NULL)
#endif /* HARDWARE_IIC */
	{
		return AHT21_ERRORRESOURCE;
	}

	(void)iic->pf_iic_init(iic->bus_context);
	instance->p_yield_instance->pf_rtos_yield(AHT21_POWER_ON_WAIT_MS);

	if (__read_status(instance, &status) != AHT21_OK)
	{
		return AHT21_ERRORRESOURCE;
	}

	if ((status & AHT21_STATUS_CALIBRATED) == 0U &&
		__send_command(instance, AHT21_REG_INITIALIZE,
						   AHT21_REG_INITIALIZE_PARAM, 0U) != AHT21_OK)
	{
		return AHT21_ERROR;
	}

	if ((status & AHT21_STATUS_CALIBRATED) == 0U)
	{
		instance->p_yield_instance->pf_rtos_yield(AHT21_INIT_WAIT_MS);
	}
    
	instance->is_inited = AHT21_INITED;

	return AHT21_OK;
}

static aht21_status_t aht21_deinit(bsp_aht21_driver_t *instance)
{
	if (instance != NULL)
	{
		instance->is_inited = AHT21_NOT_INITED;
	}

	return AHT21_OK;
}

static aht21_status_t aht21_read_id(bsp_aht21_driver_t *instance)
{
	return (instance != NULL && instance->is_inited == AHT21_INITED)
		? AHT21_OK : AHT21_ERRORRESOURCE;
}

static aht21_status_t aht21_read_temp_humi(bsp_aht21_driver_t *instance,
											float *temperature,
											float *humidity)
{
	uint8_t data[7];
	uint32_t raw_humidity;
	uint32_t raw_temperature;
	uint8_t status;

	if (instance == NULL || instance->is_inited != AHT21_INITED ||
		temperature == NULL || humidity == NULL)
	{
		return AHT21_ERRORPARAMETER;
	}

	if (__send_command(instance, AHT21_REG_TRIGGER_MEASURE,
						   AHT21_REG_MEASURE_PARAM_1,
						   AHT21_REG_MEASURE_PARAM_2) != AHT21_OK)
	{
		return AHT21_ERROR;
	}

	instance->p_yield_instance->pf_rtos_yield(AHT21_MEASURE_WAIT_MS);

	if (__read_status(instance, &status) != AHT21_OK ||
		(status & AHT21_STATUS_BUSY) != 0U)
	{
		return AHT21_ERRORTIMEOUT;
	}

	if (__read_bytes(instance, data, 7U) != AHT21_OK ||
		__crc8(data, 6U) != data[6])
	{
		return AHT21_ERROR;
	}

	raw_humidity = ((uint32_t)data[1] << 12) |
				   ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);
	raw_temperature = (((uint32_t)data[3] & 0x0FU) << 16) |
					  ((uint32_t)data[4] << 8) | data[5];
	*humidity = (float)raw_humidity * 100.0f / 1048576.0f;
	*temperature = (float)raw_temperature * 200.0f / 1048576.0f - 50.0f;

	return AHT21_OK;
}

static aht21_status_t aht21_sleep(bsp_aht21_driver_t *instance)
{
	return aht21_read_id(instance);
}

static aht21_status_t aht21_wakeup(bsp_aht21_driver_t *instance)
{
	return aht21_read_id(instance);
}

aht21_status_t aht21_inst(bsp_aht21_driver_t *instance,
						  aht21_iic_driver_interface_t *iic,
						  aht21_yield_interface_t *yield,
						  aht21_timebase_interface_t *timebase)
{
	if (instance == NULL || iic == NULL || yield == NULL ||
		yield->pf_rtos_yield == NULL)
	{
		return AHT21_ERRORPARAMETER;
	}

	if (instance->is_inited == AHT21_INITED)
	{
		return AHT21_ERRORRESOURCE;
	}

	instance->is_inited = AHT21_NOT_INITED;
	instance->p_iic_driver_instance = iic;
	instance->p_yield_instance = yield;
	instance->p_timebase_instance = timebase;
	instance->pf_inst = (aht21_status_t (*)(void * const,
											aht21_iic_driver_interface_t * const,
											aht21_yield_interface_t * const,
											aht21_timebase_interface_t * const))aht21_inst;
	instance->pf_init = (aht21_status_t (*)(void * const))aht21_init;
	instance->pf_deinit = (aht21_status_t (*)(void * const))aht21_deinit;
	instance->pf_read_id = (aht21_status_t (*)(void * const))aht21_read_id;
	instance->pf_read_temp_humi = (aht21_status_t (*)(void * const, float * const,
													   float * const))aht21_read_temp_humi;
	instance->pf_sleep = (aht21_status_t (*)(void * const))aht21_sleep;
	instance->pf_wakeup = (aht21_status_t (*)(void * const))aht21_wakeup;

	return aht21_init(instance);
}
