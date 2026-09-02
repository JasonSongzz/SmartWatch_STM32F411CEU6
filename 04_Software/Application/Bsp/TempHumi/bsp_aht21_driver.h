#ifndef __BSP_AHT21_DRIVER_H__
#define __BSP_AHT21_DRIVER_H__

#include "bsp_aht21_reg.h"

#include <stdint.h>

/* Uncomment this macro, or define it in the build system, for hardware I2C. */
//#define HARDWARE_IIC

typedef enum
{
	AHT21_OK             = 0,
	AHT21_ERROR          = 1,
	AHT21_ERRORTIMEOUT   = 2,
	AHT21_ERRORRESOURCE  = 3,
	AHT21_ERRORPARAMETER = 4,
	AHT21_ERRORNOMEMORY  = 5,
	AHT21_ERRORISR       = 6,
	AHT21_RESERVED       = 0x7FFFFFFF
} aht21_status_t;

#ifndef HARDWARE_IIC
typedef struct
{
	void *bus_context;
	aht21_status_t (*pf_iic_init)(void *);
	aht21_status_t (*pf_iic_deinit)(void *);
	aht21_status_t (*pf_iic_start)(void *);
	aht21_status_t (*pf_iic_stop)(void *);
	aht21_status_t (*pf_iic_wait_ack)(void *);
	aht21_status_t (*pf_iic_send_ack)(void *);
	aht21_status_t (*pf_iic_send_no_ack)(void *);
	aht21_status_t (*pf_iic_send_byte)(void *, uint8_t);
	aht21_status_t (*pf_iic_receive_byte)(void *, uint8_t *);
	aht21_status_t (*pf_critical_enter)(void);
	aht21_status_t (*pf_critical_exit)(void);
} aht21_iic_driver_interface_t;
#else
/* Hardware callbacks receive the 7-bit slave address (0x38), not address << 1. */
typedef struct
{
	void *bus_context;
	aht21_status_t (*pf_iic_init)(void *);
	aht21_status_t (*pf_iic_deinit)(void *);
	aht21_status_t (*pf_iic_write)(void *, uint8_t, const uint8_t *, uint16_t);
	aht21_status_t (*pf_iic_read)(void *, uint8_t, uint8_t *, uint16_t);
} aht21_iic_driver_interface_t;
#endif /* HARDWARE_IIC */

typedef struct
{
	void (*pf_rtos_yield)(uint32_t);
} aht21_yield_interface_t;

typedef struct
{
	uint32_t (*pf_get_tick_count)(void);
} aht21_timebase_interface_t;

typedef struct
{
	aht21_iic_driver_interface_t *p_iic_driver_instance;
	aht21_timebase_interface_t *p_timebase_instance;
	aht21_yield_interface_t *p_yield_instance;
	int8_t is_inited;

	aht21_status_t (*pf_inst)(void * const,
							  aht21_iic_driver_interface_t * const,
							  aht21_yield_interface_t * const,
							  aht21_timebase_interface_t * const);
	aht21_status_t (*pf_init)(void * const);
	aht21_status_t (*pf_deinit)(void * const);
	aht21_status_t (*pf_read_id)(void * const);
	aht21_status_t (*pf_read_temp_humi)(void * const, float * const, float * const);
	aht21_status_t (*pf_sleep)(void * const);
	aht21_status_t (*pf_wakeup)(void * const);
} bsp_aht21_driver_t;

aht21_status_t aht21_inst(bsp_aht21_driver_t * const p_aht21_instance,
						  aht21_iic_driver_interface_t * const p_iic_driver_instance,
						  aht21_yield_interface_t * const p_yield_instance,
						  aht21_timebase_interface_t * const p_timebase_instance);

#endif /* __BSP_AHT21_DRIVER_H__ */
