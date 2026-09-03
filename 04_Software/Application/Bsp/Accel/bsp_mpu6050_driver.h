#ifndef BSP_MPU6050_DRIVER_H
#define BSP_MPU6050_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

/* MPU6050 protocol configuration. */
#define MPU6050_I2C_ADDRESS_AD0_LOW   0x68U
#define MPU6050_I2C_ADDRESS_AD0_HIGH  0x69U
#define MPU6050_I2C_ADDRESS           MPU6050_I2C_ADDRESS_AD0_LOW
#define MPU6050_REG_SELF_TEST_X       0x0DU
#define MPU6050_REG_SMPLRT_DIV        0x19U
#define MPU6050_REG_CONFIG            0x1AU
#define MPU6050_REG_GYRO_CONFIG       0x1BU
#define MPU6050_REG_ACCEL_CONFIG      0x1CU
#define MPU6050_REG_ACCEL_XOUT_H      0x3BU
#define MPU6050_REG_TEMP_OUT_H        0x41U
#define MPU6050_REG_GYRO_XOUT_H       0x43U
#define MPU6050_REG_PWR_MGMT_1        0x6BU
#define MPU6050_REG_PWR_MGMT_2        0x6CU
#define MPU6050_REG_WHO_AM_I          0x75U
#define MPU6050_PWR1_DEVICE_RESET     0x80U
#define MPU6050_PWR1_SLEEP            0x40U
#define MPU6050_PWR1_CLKSEL_PLL_XGYRO 0x01U
#define MPU6050_WHO_AM_I_MASK         0x7EU
#define MPU6050_ACCEL_FS_2G           0x00U
#define MPU6050_ACCEL_FS_4G           0x08U
#define MPU6050_ACCEL_FS_8G           0x10U
#define MPU6050_ACCEL_FS_16G          0x18U
#define MPU6050_ACCEL_SENSITIVITY_2G  16384.0f
#define MPU6050_ACCEL_SENSITIVITY_4G  8192.0f
#define MPU6050_ACCEL_SENSITIVITY_8G  4096.0f
#define MPU6050_ACCEL_SENSITIVITY_16G 2048.0f
#define MPU6050_RESET_WAIT_MS         100U
#define MPU6050_WAKEUP_WAIT_MS        10U
#define MPU6050_DATA_LENGTH           6U

typedef enum
{
    ACCEL_OK = 0,
    ACCEL_ERROR,
    ACCEL_ERROR_TIMEOUT,
    ACCEL_ERROR_RESOURCE,
    ACCEL_ERROR_PARAMETER,
    ACCEL_ERROR_ID
} accel_status_t;

typedef struct
{
    void *bus_context;
    accel_status_t (*pf_iic_init)(void *bus_context);
    accel_status_t (*pf_iic_deinit)(void *bus_context);
    accel_status_t (*pf_iic_start)(void *bus_context);
    accel_status_t (*pf_iic_stop)(void *bus_context);
    accel_status_t (*pf_iic_wait_ack)(void *bus_context);
    accel_status_t (*pf_iic_send_ack)(void *bus_context);
    accel_status_t (*pf_iic_send_no_ack)(void *bus_context);
    accel_status_t (*pf_iic_send_byte)(void *bus_context, uint8_t data);
    accel_status_t (*pf_iic_receive_byte)(void *bus_context, uint8_t *data);
    accel_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms);
    accel_status_t (*pf_unlock)(void *bus_context);
} accel_iic_interface_t;

typedef struct
{
    void (*pf_rtos_yield)(uint32_t milliseconds);
} accel_yield_interface_t;

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} accel_raw_data_t;

typedef struct
{
    float x;
    float y;
    float z;
} accel_data_t;

typedef struct bsp_accel_driver bsp_accel_driver_t;

struct bsp_accel_driver
{
    accel_iic_interface_t *p_iic_driver_instance;
    accel_yield_interface_t *p_yield_instance;
    bool is_inited;
    uint8_t accel_config;
    float accel_sensitivity;

    accel_status_t (*pf_init)(bsp_accel_driver_t *instance);
    accel_status_t (*pf_deinit)(bsp_accel_driver_t *instance);
    accel_status_t (*pf_read_id)(bsp_accel_driver_t *instance, uint8_t *id);
    accel_status_t (*pf_read_raw_accel)(bsp_accel_driver_t *instance,
                                        accel_raw_data_t *accel);
    accel_status_t (*pf_read_accel)(bsp_accel_driver_t *instance,
                                    accel_data_t *accel);
    accel_status_t (*pf_sleep)(bsp_accel_driver_t *instance);
    accel_status_t (*pf_wakeup)(bsp_accel_driver_t *instance);
};

accel_status_t mpu6050_inst(bsp_accel_driver_t *instance,
                            accel_iic_interface_t *iic,
                            accel_yield_interface_t *yield);

#define bsp_accel_inst mpu6050_inst

#endif /* BSP_MPU6050_DRIVER_H */
