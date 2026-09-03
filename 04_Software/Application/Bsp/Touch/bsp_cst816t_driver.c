#include "bsp_cst816t_driver.h"

#include <stddef.h>

typedef touch_status_t cst816t_status_t;
typedef touch_iic_interface_t cst816t_iic_driver_interface_t;
typedef touch_yield_interface_t cst816t_yield_interface_t;
typedef touch_control_interface_t cst816t_control_interface_t;
typedef touch_point_t cst816t_point_t;
typedef bsp_touch_driver_t bsp_cst816t_driver_t;

#define CST816T_OK             TOUCH_OK
#define CST816T_ERROR          TOUCH_ERROR
#define CST816T_ERROR_PARAM    TOUCH_ERROR_PARAMETER
#define CST816T_ERROR_RESOURCE TOUCH_ERROR_RESOURCE
#define CST816T_NO_TOUCH       TOUCH_NO_TOUCH
#define CST816T_ERROR_TIMEOUT  TOUCH_ERROR_TIMEOUT

#define CST816T_IO_TIMEOUT_MS     (100U)
#define CST816T_RESET_LOW_MS      (5U)
#define CST816T_RESET_RECOVERY_MS (100U)

static cst816t_status_t first_error(cst816t_status_t current,
                                    cst816t_status_t candidate)
{
    return current == CST816T_OK ? candidate : current;
}

static cst816t_status_t lock_i2c(
    const cst816t_iic_driver_interface_t *iic)
{
    if (iic->pf_lock == NULL) return CST816T_OK;

    return iic->pf_lock(iic->bus_context, CST816T_IO_TIMEOUT_MS);
}

static cst816t_status_t unlock_i2c(
    const cst816t_iic_driver_interface_t *iic)
{
    if (iic->pf_unlock == NULL) return CST816T_OK;

    return iic->pf_unlock(iic->bus_context);
}

static cst816t_status_t send_byte_and_wait_ack(
    const cst816t_iic_driver_interface_t *iic, uint8_t data)
{
    cst816t_status_t status;

    status = iic->pf_iic_send_byte(iic->bus_context, data);

    if (status != CST816T_OK) return status;

    return iic->pf_iic_wait_ack(iic->bus_context);
}

static cst816t_status_t read_register(bsp_cst816t_driver_t *touch,
                                      uint8_t reg, uint8_t *data,
                                      uint16_t size)
{
    const cst816t_iic_driver_interface_t *iic;
    cst816t_status_t status;

    if (touch == NULL || data == NULL || size == 0U)
        return CST816T_ERROR_PARAM;

    iic = touch->p_iic_driver_instance;
    status = lock_i2c(iic);

    if (status != CST816T_OK) return status;

    status = iic->pf_iic_start(iic->bus_context);

    if (status == CST816T_OK)
        status = send_byte_and_wait_ack(
            iic, (uint8_t)(CST816T_I2C_ADDRESS << 1U));

    if (status == CST816T_OK)
        status = send_byte_and_wait_ack(iic, reg);

    if (status == CST816T_OK)
        status = iic->pf_iic_start(iic->bus_context);

    if (status == CST816T_OK)
        status = send_byte_and_wait_ack(
            iic, (uint8_t)((CST816T_I2C_ADDRESS << 1U) | 0x01U));

    for (uint16_t index = 0U; status == CST816T_OK && index < size; index++)
    {
        status = iic->pf_iic_receive_byte(iic->bus_context, &data[index]);
        if (status != CST816T_OK) break;
        status = index + 1U == size
               ? iic->pf_iic_send_no_ack(iic->bus_context)
               : iic->pf_iic_send_ack(iic->bus_context);
    }

    status = first_error(status, iic->pf_iic_stop(iic->bus_context));
    return first_error(status, unlock_i2c(iic));
}

static cst816t_status_t write_register(bsp_cst816t_driver_t *touch,
                                       uint8_t reg, const uint8_t *data,
                                       uint16_t size)
{
    const cst816t_iic_driver_interface_t *iic;
    cst816t_status_t status;

    if (touch == NULL || data == NULL || size == 0U)
        return CST816T_ERROR_PARAM;

    iic = touch->p_iic_driver_instance;
    status = lock_i2c(iic);

    if (status != CST816T_OK) return status;

    status = iic->pf_iic_start(iic->bus_context);
    if (status == CST816T_OK)
        status = send_byte_and_wait_ack(
            iic, (uint8_t)(CST816T_I2C_ADDRESS << 1U));

    if (status == CST816T_OK)
        status = send_byte_and_wait_ack(iic, reg);

    for (uint16_t index = 0U; status == CST816T_OK && index < size; index++)
        status = send_byte_and_wait_ack(iic, data[index]);

    status = first_error(status, iic->pf_iic_stop(iic->bus_context));
    return first_error(status, unlock_i2c(iic));
}

static cst816t_status_t cst816t_init(bsp_cst816t_driver_t *touch)
{
    cst816t_status_t status;

    if (touch == NULL || touch->p_iic_driver_instance == NULL ||
        touch->p_yield_instance == NULL || touch->p_control_instance == NULL ||
        touch->p_iic_driver_instance->pf_iic_init == NULL ||
        touch->p_yield_instance->pf_rtos_yield == NULL ||
        touch->p_control_instance->pf_set_reset == NULL ||
        touch->width == 0U || touch->height == 0U)

        return CST816T_ERROR_PARAM;

    touch->initialized = false;
    status = touch->p_iic_driver_instance->pf_iic_init(
        touch->p_iic_driver_instance->bus_context);

    if (status != CST816T_OK) return status;

    touch->p_control_instance->pf_set_reset(
        touch->p_control_instance->context, false);
    touch->p_yield_instance->pf_rtos_yield(CST816T_RESET_LOW_MS);
    touch->p_control_instance->pf_set_reset(
        touch->p_control_instance->context, true);
    touch->p_yield_instance->pf_rtos_yield(CST816T_RESET_RECOVERY_MS);

    status = read_register(touch, CST816T_REG_CHIP_ID, &touch->chip_id, 1U);

    if (status != CST816T_OK) return status;

    status = read_register(touch, CST816T_REG_VERSION,
                           &touch->firmware_version, 1U);
    if (status != CST816T_OK) return status;

    touch->initialized = true;

    return CST816T_OK;
}

static cst816t_status_t cst816t_read_point(bsp_cst816t_driver_t *touch,
                                           cst816t_point_t *point)
{
    uint8_t data[6];
    cst816t_status_t status;

    if (touch == NULL || point == NULL || !touch->initialized)
        return CST816T_ERROR_PARAM;

    *point = (cst816t_point_t){0};

    if (touch->p_control_instance->pf_is_interrupt_asserted != NULL &&
        !touch->p_control_instance->pf_is_interrupt_asserted(
            touch->p_control_instance->context))
        return CST816T_NO_TOUCH;

    status = read_register(touch, CST816T_REG_GESTURE_ID,
                           data, (uint16_t)sizeof(data));
    if (status != CST816T_OK) return status;

    point->gesture = data[0];
    point->fingers = data[1];

    if (point->fingers == 0U) return CST816T_NO_TOUCH;

    if (point->fingers > CST816T_MAX_POINTS) return CST816T_ERROR;

    point->x = (uint16_t)(((data[2] & 0x0FU) << 8U) | data[3]);
    point->y = (uint16_t)(((data[4] & 0x0FU) << 8U) | data[5]);
    point->event = (uint8_t)((data[2] >> 6U) & 0x03U);

    if (point->x >= touch->width || point->y >= touch->height)
    {
        *point = (cst816t_point_t){0};
        return CST816T_ERROR;
    }

    return CST816T_OK;
}

static cst816t_status_t cst816t_sleep(bsp_cst816t_driver_t *touch)
{
    const uint8_t sleep_command = 0x03U;
    cst816t_status_t status;

    if (touch == NULL || !touch->initialized) return CST816T_ERROR_PARAM;

    status = write_register(touch, CST816T_REG_SLEEP, &sleep_command, 1U);

    if (status != CST816T_OK) return status;

    touch->initialized = false;
    return CST816T_OK;
}

static cst816t_status_t cst816t_get_info(
    const bsp_cst816t_driver_t *touch, touch_info_t *info)
{
    if (touch == NULL || info == NULL || touch->width == 0U ||
        touch->height == 0U)
        return CST816T_ERROR_PARAM;

    *info = (touch_info_t){
        .width = touch->width,
        .height = touch->height,
        .max_points = CST816T_MAX_POINTS,
    };
    return CST816T_OK;
}

static cst816t_status_t cst816t_wakeup(bsp_cst816t_driver_t *touch)
{
    if (touch == NULL) return CST816T_ERROR_PARAM;

    return cst816t_init(touch);
}

static bool i2c_interface_is_valid(
    const cst816t_iic_driver_interface_t *iic)
{
    if (iic == NULL || iic->pf_iic_init == NULL ||
        ((iic->pf_lock == NULL) != (iic->pf_unlock == NULL)))
        return false;

    return iic->pf_iic_start != NULL && iic->pf_iic_stop != NULL &&
           iic->pf_iic_wait_ack != NULL && iic->pf_iic_send_ack != NULL &&
           iic->pf_iic_send_no_ack != NULL &&
           iic->pf_iic_send_byte != NULL &&
           iic->pf_iic_receive_byte != NULL;
}

cst816t_status_t cst816t_inst(
    bsp_cst816t_driver_t *touch,
    const cst816t_iic_driver_interface_t *iic,
    const cst816t_yield_interface_t *yield,
    const cst816t_control_interface_t *control)
{
    if (touch == NULL || !i2c_interface_is_valid(iic) || yield == NULL ||
        yield->pf_rtos_yield == NULL || control == NULL ||
        control->pf_set_reset == NULL)
        return CST816T_ERROR_PARAM;

    touch->p_iic_driver_instance = iic;
    touch->p_yield_instance = yield;
    touch->p_control_instance = control;
    touch->width = CST816T_COORD_WIDTH;
    touch->height = CST816T_COORD_HEIGHT;
    touch->chip_id = 0U;
    touch->firmware_version = 0U;
    touch->initialized = false;
    touch->pf_init = cst816t_init;
    touch->pf_read_point = cst816t_read_point;
    touch->pf_get_info = cst816t_get_info;
    touch->pf_sleep = cst816t_sleep;
    touch->pf_wakeup = cst816t_wakeup;

    return cst816t_init(touch);
}
