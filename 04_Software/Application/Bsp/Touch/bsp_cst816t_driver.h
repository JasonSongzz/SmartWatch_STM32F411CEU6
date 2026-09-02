#ifndef BSP_CST816T_DRIVER_H
#define BSP_CST816T_DRIVER_H

#include "bsp_cst816t_reg.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum 
{ 
    CST816T_OK = 0, 
    CST816T_ERROR = 1, 
    CST816T_ERROR_PARAM = 2,
    CST816T_ERROR_RESOURCE = 3,
    CST816T_NO_TOUCH = 4,
    CST816T_ERROR_TIMEOUT = 5
} cst816t_status_t;

#ifndef HARDWARE_IIC
typedef struct
{
	void *bus_context;
	cst816t_status_t (*pf_iic_init)(void *);
	cst816t_status_t (*pf_iic_deinit)(void *);
	cst816t_status_t (*pf_iic_start)(void *);
	cst816t_status_t (*pf_iic_stop)(void *);
	cst816t_status_t (*pf_iic_wait_ack)(void *);
	cst816t_status_t (*pf_iic_send_ack)(void *);
	cst816t_status_t (*pf_iic_send_no_ack)(void *);
	cst816t_status_t (*pf_iic_send_byte)(void *, uint8_t);
	cst816t_status_t (*pf_iic_receive_byte)(void *, uint8_t *);
	cst816t_status_t (*pf_critical_enter)(void);
	cst816t_status_t (*pf_critical_exit)(void);
} cst816t_iic_driver_interface_t;
#else
/* Hardware callbacks receive the 7-bit slave address (0x38), not address << 1. */
typedef struct
{
	void *bus_context;
	cst816t_status_t (*pf_iic_init)(void *);
	cst816t_status_t (*pf_iic_deinit)(void *);
	cst816t_status_t (*pf_iic_write)(void *, uint8_t, const uint8_t *, uint16_t);
	cst816t_status_t (*pf_iic_read)(void *, uint8_t, uint8_t *, uint16_t);
} cst816t_iic_driver_interface_t;
#endif /* HARDWARE_IIC */

typedef struct
{
	void (*pf_rtos_yield)(uint32_t);
} cst816t_yield_interface_t;

typedef struct
{
	uint32_t (*pf_get_tick_count)(void);
} cst816t_timebase_interface_t;

typedef struct {
    void *context;
    void (*pf_set_reset)(void *ctx, bool active);           // 复位引脚控制
    bool (*pf_is_interrupt_asserted)(void *ctx);           // 检测中断引脚（可选）
} cst816t_control_interface_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t gesture;
    uint8_t event;
    uint8_t fingers;
} cst816t_point_t;

typedef struct {
    const cst816t_iic_driver_interface_t *p_iic_driver_instance;
    const cst816t_timebase_interface_t *p_timebase_instance;
    const cst816t_yield_interface_t *p_yield_instance;
    const cst816t_control_interface_t *p_control_instance;
    uint16_t width;
    uint16_t height;
    bool initialized;

    cst816t_status_t (*pf_inst)(void * const,
							  cst816t_iic_driver_interface_t * const,
							  cst816t_yield_interface_t * const,
							  cst816t_timebase_interface_t * const,
                            cst816t_control_interface_t * const);

    cst816t_status_t (*pf_init)(void * const);
    cst816t_status_t (*pf_read_point)(void * const,
                                      cst816t_point_t *point);
    cst816t_status_t (*pf_sleep)(void * const);
    cst816t_status_t (*pf_wakeup)(void * const);
} bsp_cst816t_driver_t;

cst816t_status_t cst816t_inst(bsp_cst816t_driver_t * const p_cst816t_instance,
                 cst816t_iic_driver_interface_t * const p_iic_driver_instance,
                     cst816t_timebase_interface_t * const p_timebase_instance,
                           cst816t_yield_interface_t * const p_yield_instance,
                       cst816t_control_interface_t * const p_control_isntance,
                                             uint16_t width, uint16_t height);

#endif /* BSP_CST816T_DRIVER_H */
