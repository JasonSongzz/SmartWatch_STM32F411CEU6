#include "bsp_cst816t_driver.h"
#include <stddef.h>

/* 超时定义（硬件 I2C 模式可用） */
#define CST816T_IO_TIMEOUT_MS   (100U)

/* ---------- 临界区保护（与 AHT21 逻辑完全一致） ---------- */
static void __enter_bus(const cst816t_iic_driver_interface_t *iic)
{
#ifndef HARDWARE_IIC
    if (iic->pf_critical_enter != NULL) {
        (void)iic->pf_critical_enter();
    }
#else
    (void)iic;
#endif
}

static void __exit_bus(const cst816t_iic_driver_interface_t *iic)
{
#ifndef HARDWARE_IIC
    if (iic->pf_critical_exit != NULL) {
        (void)iic->pf_critical_exit();
    }
#else
    (void)iic;
#endif
}

/* ---------- I2C 读写（软件/硬件自适应） ---------- */
static cst816t_status_t __read_register(bsp_cst816t_driver_t *touch,
                                        uint8_t reg, uint8_t *data, uint16_t size)
{
    const cst816t_iic_driver_interface_t *iic = touch->p_iic_driver_instance;
    cst816t_status_t status = CST816T_OK;

    __enter_bus(iic);

#ifdef HARDWARE_IIC
    /* 硬件 I2C：需要底层支持带寄存器地址的读写 */
    status = iic->pf_iic_read(iic->bus_context, CST816T_I2C_ADDRESS,
                              reg, data, size, CST816T_IO_TIMEOUT_MS);
#else
    /* 软件 I2C：模拟时序 */
    void *ctx = iic->bus_context;

    (void)iic->pf_iic_start(ctx);
    (void)iic->pf_iic_send_byte(ctx, (uint8_t)(CST816T_I2C_ADDRESS << 1));
    if (iic->pf_iic_wait_ack(ctx) != CST816T_OK) {
        status = CST816T_ERROR;
        goto stop;
    }

    (void)iic->pf_iic_send_byte(ctx, reg);
    if (iic->pf_iic_wait_ack(ctx) != CST816T_OK) {
        status = CST816T_ERROR;
        goto stop;
    }

    (void)iic->pf_iic_start(ctx);
    (void)iic->pf_iic_send_byte(ctx, (uint8_t)((CST816T_I2C_ADDRESS << 1) | 0x01));
    if (iic->pf_iic_wait_ack(ctx) != CST816T_OK) {
        status = CST816T_ERROR;
        goto stop;
    }

    for (uint16_t i = 0; i < size; i++) {
        (void)iic->pf_iic_receive_byte(ctx, &data[i]);
        if (i == size - 1) {
            (void)iic->pf_iic_send_no_ack(ctx);
        } else {
            (void)iic->pf_iic_send_ack(ctx);
        }
    }

stop:
    (void)iic->pf_iic_stop(ctx);
#endif /* HARDWARE_IIC */

    __exit_bus(iic);
    return status;
}

static cst816t_status_t __write_register(bsp_cst816t_driver_t *touch,
                                         uint8_t reg, const uint8_t *data, uint16_t size)
{
    const cst816t_iic_driver_interface_t *iic = touch->p_iic_driver_instance;
    cst816t_status_t status = CST816T_OK;

    __enter_bus(iic);

#ifdef HARDWARE_IIC
    status = iic->pf_iic_write(iic->bus_context, CST816T_I2C_ADDRESS,
                               reg, data, size, CST816T_IO_TIMEOUT_MS);
#else
    void *ctx = iic->bus_context;

    (void)iic->pf_iic_start(ctx);
    (void)iic->pf_iic_send_byte(ctx, (uint8_t)(CST816T_I2C_ADDRESS << 1));
    if (iic->pf_iic_wait_ack(ctx) != CST816T_OK) {
        status = CST816T_ERROR;
        goto stop;
    }

    (void)iic->pf_iic_send_byte(ctx, reg);
    if (iic->pf_iic_wait_ack(ctx) != CST816T_OK) {
        status = CST816T_ERROR;
        goto stop;
    }

    for (uint16_t i = 0; i < size; i++) {
        (void)iic->pf_iic_send_byte(ctx, data[i]);
        if (iic->pf_iic_wait_ack(ctx) != CST816T_OK) {
            status = CST816T_ERROR;
            break;
        }
    }

stop:
    (void)iic->pf_iic_stop(ctx);
#endif /* HARDWARE_IIC */

    __exit_bus(iic);
    
    return status;
}

/* ---------- 驱动核心函数 ---------- */
static cst816t_status_t cst816t_init(bsp_cst816t_driver_t *touch)
{
    uint8_t chip_id;

    /* 参数校验（含 yield 实例检查，与 AHT21 一致） */
    if (touch == NULL || touch->p_iic_driver_instance == NULL ||
        touch->p_yield_instance == NULL || touch->p_control_instance == NULL ||
        touch->p_iic_driver_instance->pf_iic_init == NULL ||
        touch->p_control_instance->pf_set_reset == NULL ||
        touch->p_yield_instance->pf_rtos_yield == NULL ||
        touch->width == 0U || touch->height == 0U) {
        return CST816T_ERROR_PARAM;
    }

    (void)touch->p_iic_driver_instance->pf_iic_init(
        touch->p_iic_driver_instance->bus_context);

    /* 硬件复位：直接复用 yield 做延时，无需额外 delay 接口 */
    touch->p_control_instance->pf_set_reset(touch->p_control_instance->context, false);
    touch->p_yield_instance->pf_rtos_yield(5U);   // 保持低电平 5ms
    
    touch->p_control_instance->pf_set_reset(touch->p_control_instance->context, true);
    touch->p_yield_instance->pf_rtos_yield(100U); // 复位后等待 100ms

    /* 读 Chip ID 验证通信 */
    if (__read_register(touch, CST816T_REG_CHIP_ID, &chip_id, 1U) != CST816T_OK) {
        return CST816T_ERROR;
    }

    touch->initialized = true;

    return CST816T_OK;
}

static cst816t_status_t cst816t_read_point(bsp_cst816t_driver_t *touch,
                                           cst816t_point_t *point)
{
    uint8_t data[6];

    if (touch == NULL || point == NULL || !touch->initialized) {
        return CST816T_ERROR_PARAM;
    }

    /* 若提供了中断检测函数，则检查中断引脚 */
    if (touch->p_control_instance->pf_is_interrupt_asserted != NULL &&
        !touch->p_control_instance->pf_is_interrupt_asserted(touch->p_control_instance->context)) {
        return CST816T_NO_TOUCH;
    }

    if (__read_register(touch, CST816T_REG_GESTURE_ID, data, sizeof(data)) != CST816T_OK) {
        return CST816T_ERROR;
    }

    point->gesture = data[0];
    point->fingers = data[1];

    if (point->fingers == 0U) {
        return CST816T_NO_TOUCH;
    }

    point->x = (uint16_t)(((data[2] & 0x0FU) << 8) | data[3]);
    point->y = (uint16_t)(((data[4] & 0x0FU) << 8) | data[5]);
    point->event = (uint8_t)((data[2] >> 6) & 0x03U);

    if (point->x >= touch->width || point->y >= touch->height) {
        return CST816T_ERROR;
    }

    return CST816T_OK;
}

static cst816t_status_t cst816t_sleep(bsp_cst816t_driver_t *touch)
{
    const uint8_t sleep_cmd = 0x03U;

    if (touch == NULL || !touch->initialized) {
        return CST816T_ERROR_PARAM;
    }

    if (__write_register(touch, CST816T_REG_SLEEP, &sleep_cmd, 1U) != CST816T_OK) {
        return CST816T_ERROR;
    }

    touch->initialized = false;

    return CST816T_OK;
}

static cst816t_status_t cst816t_wakeup(bsp_cst816t_driver_t *touch)
{
    if (touch == NULL) {
        return CST816T_ERROR_PARAM;
    }

    return cst816t_init(touch);
}

/* ---------- 实例化函数（参数列表对齐 AHT21，仅增加 control 和尺寸） ---------- */
cst816t_status_t cst816t_inst(
    bsp_cst816t_driver_t * const p_cst816t_instance,
    const cst816t_iic_driver_interface_t * const p_iic_driver_instance,
    const cst816t_timebase_interface_t * const p_timebase_instance,
    const cst816t_yield_interface_t * const p_yield_instance,
    const cst816t_control_interface_t * const p_control,
    uint16_t width,
    uint16_t height)
{
    if (p_cst816t_instance == NULL || p_iic_driver_instance == NULL ||
        p_yield_instance == NULL || p_control == NULL ||
        width == 0U || height == 0U) {
        return CST816T_ERROR_PARAM;
    }

    /* 根据 I2C 模式校验必要函数指针 */
#ifdef HARDWARE_IIC
    if (p_iic_driver_instance->pf_iic_init == NULL ||
        p_iic_driver_instance->pf_iic_write == NULL ||
        p_iic_driver_instance->pf_iic_read == NULL) {
        return CST816T_ERROR_PARAM;
    }
#else
    if (p_iic_driver_instance->pf_iic_init == NULL ||
        p_iic_driver_instance->pf_iic_start == NULL ||
        p_iic_driver_instance->pf_iic_stop == NULL ||
        p_iic_driver_instance->pf_iic_send_byte == NULL ||
        p_iic_driver_instance->pf_iic_wait_ack == NULL ||
        p_iic_driver_instance->pf_iic_receive_byte == NULL ||
        p_iic_driver_instance->pf_iic_send_ack == NULL ||
        p_iic_driver_instance->pf_iic_send_no_ack == NULL) {
        return CST816T_ERROR_PARAM;
    }
#endif

    p_cst816t_instance->p_iic_driver_instance = p_iic_driver_instance;
    p_cst816t_instance->p_timebase_instance   = p_timebase_instance;
    p_cst816t_instance->p_yield_instance      = p_yield_instance;
    p_cst816t_instance->p_control_instance    = p_control;
    p_cst816t_instance->width                 = width;
    p_cst816t_instance->height                = height;
    p_cst816t_instance->initialized           = false;

    /* 绑定函数指针（与 AHT21 风格完全一致） */
    p_cst816t_instance->pf_init        = (cst816t_status_t (*)(void * const))cst816t_init;
    p_cst816t_instance->pf_read_point  = (cst816t_status_t (*)(void * const, cst816t_point_t *))cst816t_read_point;
    p_cst816t_instance->pf_sleep       = (cst816t_status_t (*)(void * const))cst816t_sleep;
    p_cst816t_instance->pf_wakeup      = (cst816t_status_t (*)(void * const))cst816t_wakeup;

    return cst816t_init(p_cst816t_instance);
}
