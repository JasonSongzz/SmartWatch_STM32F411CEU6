#ifndef BSP_ST7789T3_DRIVER_H
#define BSP_ST7789T3_DRIVER_H

#include "bsp_st7789t3_reg.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum 
{ 
    ST7789T3_OK = 0, 
    ST7789T3_ERROR = 1, 
    ST7789T3_ERROR_PARAM = 2,
    ST7789T3_ERROR_RESOURCE = 3,
    ST7789T3_ERROR_TIMEOUT = 4
} st7789t3_status_t;

typedef struct
{
    void *bus_context;
    st7789t3_status_t (*pf_spi_write)(void *bus_context,
                                      const uint8_t *data, size_t size,
                                      uint32_t timeout_ms);
    st7789t3_status_t (*pf_spi_write_dma)(void *bus_context,
                                          const uint8_t *data, size_t size);
    st7789t3_status_t (*pf_spi_wait_complete)(void *bus_context,
                                              uint32_t timeout_ms);
    st7789t3_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms);
    st7789t3_status_t (*pf_unlock)(void *bus_context);
} st7789t3_spi_driver_interface_t;

typedef struct
{
    void *context;
    void (*pf_set_cs)(void *context, bool high);
    void (*pf_set_dc)(void *context, bool data_mode);
    void (*pf_set_reset)(void *context, bool high);
    void (*pf_set_backlight)(void *context, bool on);
} st7789t3_control_interface_t;

typedef struct
{
    void *context;
    void (*pf_delay_ms)(void *context, uint32_t milliseconds);
} st7789t3_delay_interface_t;

typedef struct bsp_st7789t3 bsp_st7789t3_t;

struct bsp_st7789t3 {
    const st7789t3_spi_driver_interface_t *spi;
    const st7789t3_control_interface_t *control;
    const st7789t3_delay_interface_t *delay;
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    uint8_t madctl;
    bool initialized;

    st7789t3_status_t (*pf_init)(bsp_st7789t3_t *display);
    st7789t3_status_t (*pf_set_window)(bsp_st7789t3_t *display,
                                       uint16_t, uint16_t, uint16_t, uint16_t);
    st7789t3_status_t (*pf_write_pixels)(bsp_st7789t3_t *display,
                                         const uint8_t *, size_t);
    st7789t3_status_t (*pf_fill)(bsp_st7789t3_t *display, uint16_t);
    st7789t3_status_t (*pf_sleep)(bsp_st7789t3_t *display);
    st7789t3_status_t (*pf_wakeup)(bsp_st7789t3_t *display);
};

st7789t3_status_t st7789t3_inst(bsp_st7789t3_t *display,
                                const st7789t3_spi_driver_interface_t *spi,
                                const st7789t3_control_interface_t *control,
                                const st7789t3_delay_interface_t *delay);

#endif /* BSP_ST7789T3_DRIVER_H */
