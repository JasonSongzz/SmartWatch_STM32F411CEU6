#ifndef DRV_ADAPTER_PORT_DISPLAY_H
#define DRV_ADAPTER_PORT_DISPLAY_H

#include "drv_adapter_display.h"
#include "stm32f4xx_hal.h"

typedef struct {
    SPI_HandleTypeDef *spi;
    GPIO_TypeDef *cs_port; uint16_t cs_pin;
    GPIO_TypeDef *dc_port; uint16_t dc_pin;
    GPIO_TypeDef *reset_port; uint16_t reset_pin;
    GPIO_TypeDef *bl_port; uint16_t bl_pin;
} display_port_config_t;

st7789t3_status_t drv_adapter_port_display_register(display_drv_t *display,
                                                const display_port_config_t *config);

#endif /* DRV_ADAPTER_PORT_DISPLAY_H */
