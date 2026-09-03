#ifndef DRV_ADAPTER_PORT_ACCEL_H
#define DRV_ADAPTER_PORT_ACCEL_H

#include "drv_adapter_accel.h"
#include "osal_mutex.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* Temporary software-I2C pins. Change only these four macros for new wiring. */
#ifndef ACCEL_IIC_SCL_PORT
#define ACCEL_IIC_SCL_PORT GPIOB
#endif
#ifndef ACCEL_IIC_SCL_PIN
#define ACCEL_IIC_SCL_PIN GPIO_PIN_8
#endif
#ifndef ACCEL_IIC_SDA_PORT
#define ACCEL_IIC_SDA_PORT GPIOB
#endif
#ifndef ACCEL_IIC_SDA_PIN
#define ACCEL_IIC_SDA_PIN GPIO_PIN_9
#endif

typedef struct
{
    void *iic_bus;
    osal_mutex_handle_t bus_mutex;
    float accel_scale;
} accel_port_config_t;

bool drv_adapter_port_accel_register(uint32_t index,
                                     const accel_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_ACCEL_H */
