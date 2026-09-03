#ifndef DRV_ADAPTER_PORT_TOUCH_H
#define DRV_ADAPTER_PORT_TOUCH_H

#include "drv_adapter_touch.h"
#include "osal_mutex.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

/* Temporary software-I2C/control pins. Change these macros for new wiring. */
#ifndef TOUCH_IIC_SCL_PORT
#define TOUCH_IIC_SCL_PORT GPIOA
#endif
#ifndef TOUCH_IIC_SCL_PIN
#define TOUCH_IIC_SCL_PIN GPIO_PIN_8
#endif
#ifndef TOUCH_IIC_SDA_PORT
#define TOUCH_IIC_SDA_PORT GPIOA
#endif
#ifndef TOUCH_IIC_SDA_PIN
#define TOUCH_IIC_SDA_PIN GPIO_PIN_9
#endif
#ifndef TOUCH_RESET_PORT
#define TOUCH_RESET_PORT GPIOA
#endif
#ifndef TOUCH_RESET_PIN
#define TOUCH_RESET_PIN GPIO_PIN_10
#endif
#ifndef TOUCH_INTERRUPT_PORT
#define TOUCH_INTERRUPT_PORT GPIOA
#endif
#ifndef TOUCH_INTERRUPT_PIN
#define TOUCH_INTERRUPT_PIN GPIO_PIN_11
#endif

typedef struct
{
    void *iic_bus;
    osal_mutex_handle_t bus_mutex;
    GPIO_TypeDef *reset_port;
    uint16_t reset_pin;
    GPIO_TypeDef *interrupt_port;
    uint16_t interrupt_pin;
    bool interrupt_active_low;
} touch_port_config_t;

bool drv_adapter_port_touch_register(uint32_t index,
                                     const touch_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_TOUCH_H */
