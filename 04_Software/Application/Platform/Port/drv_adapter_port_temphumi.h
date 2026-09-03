#ifndef DRV_ADAPTER_PORT_TEMPHUMI_H
#define DRV_ADAPTER_PORT_TEMPHUMI_H

#include "drv_adapter_temphumi.h"
#include "osal_mutex.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* Temporary software-I2C pins. Change only these four macros for new wiring. */
#ifndef TEMP_HUMI_IIC_SCL_PORT
#define TEMP_HUMI_IIC_SCL_PORT GPIOB
#endif
#ifndef TEMP_HUMI_IIC_SCL_PIN
#define TEMP_HUMI_IIC_SCL_PIN GPIO_PIN_6
#endif
#ifndef TEMP_HUMI_IIC_SDA_PORT
#define TEMP_HUMI_IIC_SDA_PORT GPIOB
#endif
#ifndef TEMP_HUMI_IIC_SDA_PIN
#define TEMP_HUMI_IIC_SDA_PIN GPIO_PIN_7
#endif

typedef struct
{
    void *iic_bus;
    osal_mutex_handle_t bus_mutex;
    float temp_scale;
} temphumi_port_config_t;

bool drv_adapter_port_temphumi_register(
    uint32_t index, const temphumi_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_TEMPHUMI_H */
