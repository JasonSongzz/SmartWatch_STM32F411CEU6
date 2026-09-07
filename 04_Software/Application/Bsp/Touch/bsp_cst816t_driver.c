#include "bsp_cst816t_driver.h"

#include <stddef.h>

#define CST816T_IO_TIMEOUT_MS     (100U)
#define CST816T_RESET_LOW_MS      (5U)
#define CST816T_RESET_RECOVERY_MS (100U)

static touch_status_t cst816t_first_error(touch_status_t current,
                                    touch_status_t candidate)
{
    return current == TOUCH_OK ? candidate : current;
}

static touch_status_t cst816t_lock_i2c(
    const touch_iic_interface_t *iic)
{
    if (iic->pf_lock == NULL) return TOUCH_OK;

    return iic->pf_lock(iic->bus_context, CST816T_IO_TIMEOUT_MS);
}

static touch_status_t cst816t_unlock_i2c(
    const touch_iic_interface_t *iic)
{
    if (iic->pf_unlock == NULL) return TOUCH_OK;

    return iic->pf_unlock(iic->bus_context);
}

static touch_status_t cst816t_send_byte_and_wait_ack(
    const touch_iic_interface_t *iic, uint8_t data)
{
    touch_status_t status;

    status = iic->pf_iic_send_byte(iic->bus_context, data);

    if (status != TOUCH_OK) return status;

    return iic->pf_iic_wait_ack(iic->bus_context);
}

static touch_status_t cst816t_read_register(bsp_touch_driver_t *touch,
                                      uint8_t reg, uint8_t *data,
                                      uint16_t size)
{
    const touch_iic_interface_t *iic;
    touch_status_t status;

    if (touch == NULL || data == NULL || size == 0U)
        return TOUCH_ERROR_PARAMETER;

    iic = touch->p_iic_driver_instance;
    status = cst816t_lock_i2c(iic);

    if (status != TOUCH_OK) return status;

    status = iic->pf_iic_start(iic->bus_context);

    if (status == TOUCH_OK)
        status = cst816t_send_byte_and_wait_ack(
            iic, (uint8_t)(CST816T_I2C_ADDRESS << 1U));

    if (status == TOUCH_OK)
        status = cst816t_send_byte_and_wait_ack(iic, reg);

    if (status == TOUCH_OK)
        status = iic->pf_iic_start(iic->bus_context);

    if (status == TOUCH_OK)
        status = cst816t_send_byte_and_wait_ack(
            iic, (uint8_t)((CST816T_I2C_ADDRESS << 1U) | 0x01U));

    for (uint16_t index = 0U; status == TOUCH_OK && index < size; index++)
    {
        status = iic->pf_iic_receive_byte(iic->bus_context, &data[index]);
        if (status != TOUCH_OK) break;
        status = index + 1U == size
               ? iic->pf_iic_send_no_ack(iic->bus_context)
               : iic->pf_iic_send_ack(iic->bus_context);
    }

    status = cst816t_first_error(status, iic->pf_iic_stop(iic->bus_context));
    return cst816t_first_error(status, cst816t_unlock_i2c(iic));
}

static touch_status_t cst816t_write_register(bsp_touch_driver_t *touch,
                                       uint8_t reg, const uint8_t *data,
                                       uint16_t size)
{
    const touch_iic_interface_t *iic;
    touch_status_t status;

    if (touch == NULL || data == NULL || size == 0U)
        return TOUCH_ERROR_PARAMETER;

    iic = touch->p_iic_driver_instance;
    status = cst816t_lock_i2c(iic);

    if (status != TOUCH_OK) return status;

    status = iic->pf_iic_start(iic->bus_context);
    if (status == TOUCH_OK)
        status = cst816t_send_byte_and_wait_ack(
            iic, (uint8_t)(CST816T_I2C_ADDRESS << 1U));

    if (status == TOUCH_OK)
        status = cst816t_send_byte_and_wait_ack(iic, reg);

    for (uint16_t index = 0U; status == TOUCH_OK && index < size; index++)
        status = cst816t_send_byte_and_wait_ack(iic, data[index]);

    status = cst816t_first_error(status, iic->pf_iic_stop(iic->bus_context));
    return cst816t_first_error(status, cst816t_unlock_i2c(iic));
}

static touch_status_t cst816t_init(bsp_touch_driver_t *touch)
{
    touch_status_t status;

    if (touch == NULL || touch->p_iic_driver_instance == NULL ||
        touch->p_yield_instance == NULL || touch->p_control_instance == NULL ||
        touch->p_iic_driver_instance->pf_iic_init == NULL ||
        touch->p_yield_instance->pf_rtos_yield == NULL ||
        touch->p_control_instance->pf_set_reset == NULL ||
        touch->width == 0U || touch->height == 0U)

        return TOUCH_ERROR_PARAMETER;

    touch->initialized = false;
    status = touch->p_iic_driver_instance->pf_iic_init(
        touch->p_iic_driver_instance->bus_context);

    if (status != TOUCH_OK) return status;

    touch->p_control_instance->pf_set_reset(
        touch->p_control_instance->context, false);
    touch->p_yield_instance->pf_rtos_yield(CST816T_RESET_LOW_MS);
    touch->p_control_instance->pf_set_reset(
        touch->p_control_instance->context, true);
    touch->p_yield_instance->pf_rtos_yield(CST816T_RESET_RECOVERY_MS);

    status = cst816t_read_register(touch, CST816T_REG_CHIP_ID, &touch->chip_id, 1U);

    if (status != TOUCH_OK) return status;

    status = cst816t_read_register(touch, CST816T_REG_VERSION,
                           &touch->firmware_version, 1U);
    if (status != TOUCH_OK) return status;

    touch->initialized = true;

    return TOUCH_OK;
}

static touch_status_t cst816t_read_point(bsp_touch_driver_t *touch,
                                           touch_point_t *point)
{
    uint8_t data[6];
    touch_status_t status;

    if (touch == NULL || point == NULL || !touch->initialized)
        return TOUCH_ERROR_PARAMETER;

    *point = (touch_point_t){0};

    status = cst816t_read_register(touch, CST816T_REG_GESTURE_ID,
                           data, (uint16_t)sizeof(data));
    if (status != TOUCH_OK) return status;

    point->gesture = data[0];
    point->fingers = data[1];

    if (point->fingers == 0U) return TOUCH_NO_TOUCH;

    if (point->fingers > CST816T_MAX_POINTS) return TOUCH_ERROR;

    point->x = (uint16_t)(((data[2] & 0x0FU) << 8U) | data[3]);
    point->y = (uint16_t)(((data[4] & 0x0FU) << 8U) | data[5]);
    point->event = (uint8_t)((data[2] >> 6U) & 0x03U);

    return TOUCH_OK;
}

static touch_status_t cst816t_sleep(bsp_touch_driver_t *touch)
{
    const uint8_t sleep_command = 0x03U;
    touch_status_t status;

    if (touch == NULL || !touch->initialized) return TOUCH_ERROR_PARAMETER;

    status = cst816t_write_register(touch, CST816T_REG_SLEEP, &sleep_command, 1U);

    if (status != TOUCH_OK) return status;

    touch->initialized = false;
    return TOUCH_OK;
}

static touch_status_t cst816t_get_info(
    const bsp_touch_driver_t *touch, touch_info_t *info)
{
    if (touch == NULL || info == NULL || touch->width == 0U ||
        touch->height == 0U)
        return TOUCH_ERROR_PARAMETER;

    *info = (touch_info_t){
        .width = touch->width,
        .height = touch->height,
        .max_points = CST816T_MAX_POINTS,
    };
    return TOUCH_OK;
}

static touch_status_t cst816t_wakeup(bsp_touch_driver_t *touch)
{
    if (touch == NULL) return TOUCH_ERROR_PARAMETER;

    return cst816t_init(touch);
}

static bool cst816t_i2c_interface_is_valid(
    const touch_iic_interface_t *iic)
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

touch_status_t cst816t_inst(
    bsp_touch_driver_t *touch,
    const touch_iic_interface_t *iic,
    const touch_yield_interface_t *yield,
    const touch_control_interface_t *control)
{
    if (touch == NULL || !cst816t_i2c_interface_is_valid(iic) || yield == NULL ||
        yield->pf_rtos_yield == NULL || control == NULL ||
        control->pf_set_reset == NULL)
        return TOUCH_ERROR_PARAMETER;

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
