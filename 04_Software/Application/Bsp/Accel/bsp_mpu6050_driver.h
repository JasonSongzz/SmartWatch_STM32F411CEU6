#ifndef __BSP_MPU6050_DRIVER_H__
#define __BSP_MPU6050_DRIVER_H__

#include "bsp_mpu6050_reg.h"
#include <stdint.h>

//#define HARDWARE_IIC

typedef enum
{
    MPU6050_OK             = 0,
    MPU6050_ERROR          = 1,
    MPU6050_ERRORTIMEOUT   = 2,
    MPU6050_ERRORRESOURCE  = 3,
    MPU6050_ERRORPARAMETER = 4,
    MPU6050_ERRORNOMEMORY  = 5,
    MPU6050_ERRORISR       = 6,
    MPU6050_ERRORID        = 7,
    MPU6050_RESERVED       = 0x7FFFFFFF
} mpu6050_status_t;

#ifndef HARDWARE_IIC
typedef struct
{
    void *bus_context;
    mpu6050_status_t (*pf_iic_init)(void *);
    mpu6050_status_t (*pf_iic_deinit)(void *);
    mpu6050_status_t (*pf_iic_start)(void *);
    mpu6050_status_t (*pf_iic_stop)(void *);
    mpu6050_status_t (*pf_iic_wait_ack)(void *);
    mpu6050_status_t (*pf_iic_send_ack)(void *);
    mpu6050_status_t (*pf_iic_send_no_ack)(void *);
    mpu6050_status_t (*pf_iic_send_byte)(void *, uint8_t);
    mpu6050_status_t (*pf_iic_receive_byte)(void *, uint8_t *);
    mpu6050_status_t (*pf_critical_enter)(void);
    mpu6050_status_t (*pf_critical_exit)(void);
} mpu6050_iic_driver_interface_t;
#else
typedef struct
{
    void *bus_context;
    mpu6050_status_t (*pf_iic_init)(void *);
    mpu6050_status_t (*pf_iic_deinit)(void *);
    mpu6050_status_t (*pf_iic_write)(void *, uint8_t, uint8_t, const uint8_t *, uint16_t);
    mpu6050_status_t (*pf_iic_read)(void *, uint8_t, uint8_t, uint8_t *, uint16_t);
} mpu6050_iic_driver_interface_t;
#endif /* HARDWARE_IIC */

typedef struct
{
    void (*pf_rtos_yield)(uint32_t);
} mpu6050_yield_interface_t;

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} mpu6050_raw_accel_t;

typedef struct
{
    float x;
    float y;
    float z;
} mpu6050_accel_t;

typedef struct
{
    mpu6050_iic_driver_interface_t *p_iic_driver_instance;
    mpu6050_yield_interface_t *p_yield_instance;
    int8_t is_inited;
    uint8_t accel_config;
    float accel_sensitivity;

    mpu6050_status_t (*pf_init)(void * const);
    mpu6050_status_t (*pf_deinit)(void * const);
    mpu6050_status_t (*pf_read_id)(void * const, uint8_t * const);
    mpu6050_status_t (*pf_read_raw_accel)(void * const, mpu6050_raw_accel_t * const);
    mpu6050_status_t (*pf_read_accel)(void * const, mpu6050_accel_t * const);
    mpu6050_status_t (*pf_sleep)(void * const);
    mpu6050_status_t (*pf_wakeup)(void * const);
} bsp_mpu6050_driver_t;

mpu6050_status_t mpu6050_inst(bsp_mpu6050_driver_t * const,
                              mpu6050_iic_driver_interface_t * const,
                              mpu6050_yield_interface_t * const);

#endif /* __BSP_MPU6050_DRIVER_H__ */
