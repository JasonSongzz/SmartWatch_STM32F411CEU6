#ifndef BSP_AHT21_DRIVER_H
#define BSP_AHT21_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

/* AHT21 protocol configuration. */
#define AHT21_REG_I2C_ADDRESS         0x38U
#define AHT21_REG_INITIALIZE          0xBEU
#define AHT21_REG_TRIGGER_MEASURE     0xACU
#define AHT21_REG_SOFT_RESET          0xBAU
#define AHT21_REG_INITIALIZE_PARAM    0x08U
#define AHT21_REG_MEASURE_PARAM_1     0x33U
#define AHT21_REG_MEASURE_PARAM_2     0x00U
#define AHT21_STATUS_BUSY             0x80U
#define AHT21_STATUS_CALIBRATED       0x08U
#define AHT21_MEASURE_WAIT_MS         80U
#define AHT21_INIT_WAIT_MS            10U
#define AHT21_POWER_ON_WAIT_MS        40U

typedef enum
{
    TEMP_HUMI_OK = 0,
    TEMP_HUMI_ERROR,
    TEMP_HUMI_ERROR_TIMEOUT,
    TEMP_HUMI_ERROR_RESOURCE,
    TEMP_HUMI_ERROR_PARAMETER
} temp_humi_status_t;

typedef struct
{
    void *bus_context;
    temp_humi_status_t (*pf_iic_init)(void *bus_context);
    temp_humi_status_t (*pf_iic_deinit)(void *bus_context);
    temp_humi_status_t (*pf_iic_start)(void *bus_context);
    temp_humi_status_t (*pf_iic_stop)(void *bus_context);
    temp_humi_status_t (*pf_iic_wait_ack)(void *bus_context);
    temp_humi_status_t (*pf_iic_send_ack)(void *bus_context);
    temp_humi_status_t (*pf_iic_send_no_ack)(void *bus_context);
    temp_humi_status_t (*pf_iic_send_byte)(void *bus_context, uint8_t data);
    temp_humi_status_t (*pf_iic_receive_byte)(void *bus_context,
                                               uint8_t *data);
    temp_humi_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms);
    temp_humi_status_t (*pf_unlock)(void *bus_context);
} temp_humi_iic_interface_t;

typedef struct
{
    void (*pf_rtos_yield)(uint32_t milliseconds);
} temp_humi_yield_interface_t;

typedef struct bsp_temp_humi_driver bsp_temp_humi_driver_t;

struct bsp_temp_humi_driver
{
    temp_humi_iic_interface_t *p_iic_driver_instance;
    temp_humi_yield_interface_t *p_yield_instance;
    uint8_t i2c_address;
    bool is_inited;

    temp_humi_status_t (*pf_init)(bsp_temp_humi_driver_t *instance);
    temp_humi_status_t (*pf_deinit)(bsp_temp_humi_driver_t *instance);
    temp_humi_status_t (*pf_read_temp_humi)(
        bsp_temp_humi_driver_t *instance, float *temperature,
        float *humidity);
    temp_humi_status_t (*pf_sleep)(bsp_temp_humi_driver_t *instance);
    temp_humi_status_t (*pf_wakeup)(bsp_temp_humi_driver_t *instance);
};

temp_humi_status_t aht21_inst(bsp_temp_humi_driver_t *instance,
                              temp_humi_iic_interface_t *iic,
                              temp_humi_yield_interface_t *yield);

/* The handler calls only this model-independent assembly entry. */
#define bsp_temp_humi_inst aht21_inst

#endif /* BSP_AHT21_DRIVER_H */
