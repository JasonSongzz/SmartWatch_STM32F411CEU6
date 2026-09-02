#ifndef DRV_ADAPTER_PORT_TOUCH_H
#define DRV_ADAPTER_PORT_TOUCH_H

#include "drv_adapter_port_i2c.h"
#include "drv_adapter_touch.h"
#include "stm32f4xx_hal.h"

typedef struct {
    drv_soft_i2c_port_t *i2c_port;
    GPIO_TypeDef *reset_port;
    uint16_t reset_pin;
    GPIO_TypeDef *interrupt_port;
    uint16_t interrupt_pin;
    bool interrupt_active_low;
} touch_port_config_t;

cst816t_status_t drv_adapter_port_touch_register(touch_drv_t *touch,
                                                  const touch_port_config_t *config,
                                                  uint16_t width, uint16_t height);

#endif /* DRV_ADAPTER_PORT_TOUCH_H */
