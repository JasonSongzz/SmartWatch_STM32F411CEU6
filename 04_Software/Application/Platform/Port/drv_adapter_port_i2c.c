#include "drv_adapter_port_i2c.h"

#include <limits.h>
#include <stddef.h>

static void enable_gpio_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
#ifdef GPIOD
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
#ifdef GPIOE
    else if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
#ifdef GPIOF
    else if (port == GPIOF) __HAL_RCC_GPIOF_CLK_ENABLE();
#endif
#ifdef GPIOG
    else if (port == GPIOG) __HAL_RCC_GPIOG_CLK_ENABLE();
#endif
#ifdef GPIOH
    else if (port == GPIOH) __HAL_RCC_GPIOH_CLK_ENABLE();
#endif
}

bool drv_adapter_port_soft_i2c_lock(drv_soft_i2c_port_t *port,
                                    uint32_t timeout_ms)
{
    if (port == NULL || port->mutex == NULL) return false;
    return osal_mutex_take(port->mutex, timeout_ms) == OSAL_SUCCESS;
}

void drv_adapter_port_soft_i2c_unlock(drv_soft_i2c_port_t *port)
{
    if (port != NULL && port->mutex != NULL)
        (void)osal_mutex_give(port->mutex);
}

bool drv_adapter_port_soft_i2c_mem_read(drv_soft_i2c_port_t *port,
                                        uint8_t address, uint8_t reg,
                                        uint8_t *data, size_t size,
                                        uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (port == NULL || !port->initialized || data == NULL ||
        size == 0U || size > UINT8_MAX)
        return false;

    return IIC_Read_Multi_Byte(&port->bus, address, reg,
                               (uint8_t)size, data) == 0U;
}

bool drv_adapter_port_soft_i2c_mem_write(drv_soft_i2c_port_t *port,
                                         uint8_t address, uint8_t reg,
                                         const uint8_t *data, size_t size,
                                         uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (port == NULL || !port->initialized || data == NULL ||
        size == 0U || size > UINT8_MAX)
        return false;

    return IIC_Write_Multi_Byte(&port->bus, address, reg,
                                (uint8_t)size, (uint8_t *)data) == 0U;
}

bool drv_adapter_port_soft_i2c_init(drv_soft_i2c_port_t *port,
                                    const iic_bus_t *bus)
{
    if (port == NULL || bus == NULL || bus->IIC_SDA_PORT == NULL ||
        bus->IIC_SCL_PORT == NULL || bus->IIC_SDA_PIN == 0U ||
        bus->IIC_SCL_PIN == 0U)
        return false;

    if (port->initialized) return true;

    port->bus = *bus;
    if (osal_mutex_create(&port->mutex) != OSAL_SUCCESS) return false;

    enable_gpio_clock(port->bus.IIC_SDA_PORT);
    enable_gpio_clock(port->bus.IIC_SCL_PORT);
    IICInit(&port->bus);
    port->initialized = true;
    return true;
}
