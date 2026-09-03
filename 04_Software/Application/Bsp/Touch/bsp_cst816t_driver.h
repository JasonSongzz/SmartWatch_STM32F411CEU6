#ifndef BSP_CST816T_DRIVER_H
#define BSP_CST816T_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

/* CST816T protocol configuration. */
#define CST816T_I2C_ADDRESS       0x15U
#define CST816T_REG_GESTURE_ID    0x01U
#define CST816T_REG_FINGER_COUNT  0x02U
#define CST816T_REG_XPOS_HIGH     0x03U
#define CST816T_REG_XPOS_LOW      0x04U
#define CST816T_REG_YPOS_HIGH     0x05U
#define CST816T_REG_YPOS_LOW      0x06U
#define CST816T_REG_CHIP_ID       0xA7U
#define CST816T_REG_VERSION       0xA9U
#define CST816T_REG_SLEEP         0xE5U
#define CST816T_MAX_POINTS        1U
#define CST816T_COORD_WIDTH       240U
#define CST816T_COORD_HEIGHT      280U

typedef enum
{
    TOUCH_OK = 0,
    TOUCH_ERROR,
    TOUCH_ERROR_PARAMETER,
    TOUCH_ERROR_RESOURCE,
    TOUCH_NO_TOUCH,
    TOUCH_ERROR_TIMEOUT
} touch_status_t;

typedef struct
{
    void *bus_context;
    touch_status_t (*pf_iic_init)(void *bus_context);
    touch_status_t (*pf_iic_deinit)(void *bus_context);
    touch_status_t (*pf_iic_start)(void *bus_context);
    touch_status_t (*pf_iic_stop)(void *bus_context);
    touch_status_t (*pf_iic_wait_ack)(void *bus_context);
    touch_status_t (*pf_iic_send_ack)(void *bus_context);
    touch_status_t (*pf_iic_send_no_ack)(void *bus_context);
    touch_status_t (*pf_iic_send_byte)(void *bus_context, uint8_t data);
    touch_status_t (*pf_iic_receive_byte)(void *bus_context, uint8_t *data);
    touch_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms);
    touch_status_t (*pf_unlock)(void *bus_context);
} touch_iic_interface_t;

typedef struct
{
    void (*pf_rtos_yield)(uint32_t milliseconds);
} touch_yield_interface_t;

typedef struct
{
    void *context;
    void (*pf_set_reset)(void *context, bool high);
    bool (*pf_is_interrupt_asserted)(void *context);
} touch_control_interface_t;

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t gesture;
    uint8_t event;
    uint8_t fingers;
} touch_point_t;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint8_t max_points;
} touch_info_t;

typedef struct bsp_touch_driver bsp_touch_driver_t;

struct bsp_touch_driver
{
    const touch_iic_interface_t *p_iic_driver_instance;
    const touch_yield_interface_t *p_yield_instance;
    const touch_control_interface_t *p_control_instance;
    uint16_t width;
    uint16_t height;
    uint8_t chip_id;
    uint8_t firmware_version;
    bool initialized;

    touch_status_t (*pf_init)(bsp_touch_driver_t *touch);
    touch_status_t (*pf_read_point)(bsp_touch_driver_t *touch,
                                    touch_point_t *point);
    touch_status_t (*pf_get_info)(const bsp_touch_driver_t *touch,
                                  touch_info_t *info);
    touch_status_t (*pf_sleep)(bsp_touch_driver_t *touch);
    touch_status_t (*pf_wakeup)(bsp_touch_driver_t *touch);
};

touch_status_t cst816t_inst(bsp_touch_driver_t *touch,
                            const touch_iic_interface_t *iic,
                            const touch_yield_interface_t *yield,
                            const touch_control_interface_t *control);

#define bsp_touch_inst cst816t_inst

#endif /* BSP_CST816T_DRIVER_H */
