#ifndef BSP_ST7789T3_DRIVER_H
#define BSP_ST7789T3_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ST7789T3 protocol configuration. */
#define ST7789T3_CMD_SWRESET           0x01U
#define ST7789T3_CMD_SLPIN             0x10U
#define ST7789T3_CMD_SLPOUT            0x11U
#define ST7789T3_CMD_MADCTL            0x36U
#define ST7789T3_CMD_COLMOD            0x3AU
#define ST7789T3_CMD_CASET             0x2AU
#define ST7789T3_CMD_RASET             0x2BU
#define ST7789T3_CMD_RAMWR             0x2CU
#define ST7789T3_CMD_DISPON            0x29U
#define ST7789T3_CMD_DISPOFF           0x28U
#define ST7789T3_PIXEL_FORMAT_RGB565   0x55U
#define ST7789T3_WIDTH                 240U
#define ST7789T3_HEIGHT                280U
#define ST7789T3_X_OFFSET              0U
#define ST7789T3_Y_OFFSET              0U
#define ST7789T3_MADCTL                0x00U
#define ST7789T3_BITS_PER_PIXEL        16U
#define ST7789T3_REQUIRES_BYTE_SWAP    true

typedef enum
{
    DISPLAY_OK = 0,
    DISPLAY_ERROR,
    DISPLAY_ERROR_PARAMETER,
    DISPLAY_ERROR_RESOURCE,
    DISPLAY_ERROR_TIMEOUT
} display_status_t;

typedef struct
{
    void *bus_context;
    display_status_t (*pf_spi_write)(void *bus_context,
                                     const uint8_t *data, size_t size,
                                     uint32_t timeout_ms);
    display_status_t (*pf_spi_write_dma)(void *bus_context,
                                         const uint8_t *data, size_t size);
    display_status_t (*pf_spi_wait_complete)(void *bus_context,
                                             uint32_t timeout_ms);
    display_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms);
    display_status_t (*pf_unlock)(void *bus_context);
} display_spi_interface_t;

typedef struct
{
    void *context;
    void (*pf_set_cs)(void *context, bool high);
    void (*pf_set_dc)(void *context, bool data_mode);
    void (*pf_set_reset)(void *context, bool high);
    void (*pf_set_backlight)(void *context, bool on);
} display_control_interface_t;

typedef struct
{
    void *context;
    void (*pf_delay_ms)(void *context, uint32_t milliseconds);
} display_delay_interface_t;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    uint8_t bits_per_pixel;
    bool requires_byte_swap;
} display_info_t;

typedef struct bsp_display_driver bsp_display_driver_t;

struct bsp_display_driver
{
    const display_spi_interface_t *spi;
    const display_control_interface_t *control;
    const display_delay_interface_t *delay;
    uint16_t width;
    uint16_t height;
    uint16_t x_offset;
    uint16_t y_offset;
    uint8_t madctl;
    bool initialized;

    display_status_t (*pf_init)(bsp_display_driver_t *display);
    display_status_t (*pf_set_window)(bsp_display_driver_t *display,
                                      uint16_t x0, uint16_t y0,
                                      uint16_t x1, uint16_t y1);
    display_status_t (*pf_write_pixels)(bsp_display_driver_t *display,
                                        const uint8_t *pixels, size_t size);
    display_status_t (*pf_fill)(bsp_display_driver_t *display, uint16_t color);
    display_status_t (*pf_get_info)(const bsp_display_driver_t *display,
                                    display_info_t *info);
    display_status_t (*pf_sleep)(bsp_display_driver_t *display);
    display_status_t (*pf_wakeup)(bsp_display_driver_t *display);
};

display_status_t st7789t3_inst(bsp_display_driver_t *display,
                               const display_spi_interface_t *spi,
                               const display_control_interface_t *control,
                               const display_delay_interface_t *delay);

#define bsp_display_inst st7789t3_inst

#endif /* BSP_ST7789T3_DRIVER_H */
