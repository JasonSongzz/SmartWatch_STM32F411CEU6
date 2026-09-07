#ifndef BSP_SHT30_DRIVER_H
#define BSP_SHT30_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

/* SHT30 protocol configuration. */
#define SHT30_REG_I2C_ADDRESS_DEFAULT      0x44U
#define SHT30_REG_I2C_ADDRESS_ALTERNATE    0x45U
#define SHT30_CMD_MEASURE_HIGH_REPEATABILITY 0x2400U
#define SHT30_CMD_SOFT_RESET               0x30A2U
#define SHT30_CMD_READ_STATUS              0xF32DU
#define SHT30_CMD_CLEAR_STATUS             0x3041U
#define SHT30_POWER_ON_WAIT_MS             2U
#define SHT30_RESET_WAIT_MS                2U
#define SHT30_MEASURE_WAIT_MS              16U
#define SHT30_CRC8_POLYNOMIAL              0x31U
#define SHT30_CRC8_INITIAL                 0xFFU

/** @brief 温湿度 BSP 统一返回状态。 */
typedef enum
{
    TEMP_HUMI_OK = 0,          /**< 操作成功。 */
    TEMP_HUMI_DATA_NOT_READY,  /**< 尚无一组完整的有效数据。 */
    TEMP_HUMI_ERROR,           /**< 通信、校验或测量数据错误。 */
    TEMP_HUMI_ERROR_TIMEOUT,   /**< 总线应答或互斥锁等待超时。 */
    TEMP_HUMI_ERROR_RESOURCE,  /**< 驱动未初始化或资源不可用。 */
    TEMP_HUMI_ERROR_PARAMETER  /**< 传入了空指针或非法参数。 */
} temp_humi_status_t;

/**
 * @brief 温湿度驱动使用的 I2C 抽象接口。
 *
 * 具体 GPIO、I2C HAL 和 OS 互斥锁由 Platform/Port 注入。pf_lock 与
 * pf_unlock 保护一次完整总线事务，两者必须成对实现或同时为空。
 */
typedef struct
{
    void *bus_context; /**< Port 持有并原样回传的总线上下文。 */
    temp_humi_status_t (*pf_iic_init)(void *bus_context); /**< 初始化 I2C 总线。 */
    temp_humi_status_t (*pf_iic_deinit)(void *bus_context); /**< 可选的反初始化。 */
    temp_humi_status_t (*pf_iic_start)(void *bus_context); /**< 产生 START 条件。 */
    temp_humi_status_t (*pf_iic_stop)(void *bus_context); /**< 产生 STOP 条件。 */
    temp_humi_status_t (*pf_iic_wait_ack)(void *bus_context); /**< 等待从机 ACK。 */
    temp_humi_status_t (*pf_iic_send_ack)(void *bus_context); /**< 主机发送 ACK。 */
    temp_humi_status_t (*pf_iic_send_no_ack)(void *bus_context); /**< 主机发送 NACK。 */
    temp_humi_status_t (*pf_iic_send_byte)(void *bus_context, uint8_t data); /**< 发送一字节。 */
    temp_humi_status_t (*pf_iic_receive_byte)(void *bus_context,
                                               uint8_t *data); /**< 接收一字节。 */
    temp_humi_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms); /**< 锁定整条总线事务。 */
    temp_humi_status_t (*pf_unlock)(void *bus_context); /**< 释放总线事务锁。 */
} temp_humi_iic_interface_t;

/** @brief BSP 所需的阻塞让出接口，由 Port 映射到 OSAL 延时。 */
typedef struct
{
    void (*pf_rtos_yield)(uint32_t milliseconds); /**< 至少让出指定毫秒数。 */
} temp_humi_yield_interface_t;

/** @brief 允许驱动操作表回调引用自身实例的前向类型。 */
typedef struct bsp_temp_humi_driver bsp_temp_humi_driver_t;

/**
 * @brief BSP 温湿度传感器实例及型号无关操作表。
 *
 * Handler 只依赖此结构；切换到 AHT21 等器件时应保持所有回调语义一致。
 */
struct bsp_temp_humi_driver
{
    temp_humi_iic_interface_t *p_iic_driver_instance; /**< Port 注入的 I2C 接口。 */
    temp_humi_yield_interface_t *p_yield_instance; /**< Port 注入的 OS 让出接口。 */
    uint8_t i2c_address; /**< 当前器件的 7 位 I2C 地址。 */
    bool is_inited; /**< 硬件初始化成功标志。 */

    temp_humi_status_t (*pf_init)(bsp_temp_humi_driver_t *instance); /**< 初始化并校验器件状态。 */
    temp_humi_status_t (*pf_deinit)(bsp_temp_humi_driver_t *instance); /**< 释放器件资源。 */
    temp_humi_status_t (*pf_read_temp_humi)(
        bsp_temp_humi_driver_t *instance, float *temperature,
        float *humidity); /**< 单次读取摄氏温度和相对湿度百分比。 */
    temp_humi_status_t (*pf_sleep)(bsp_temp_humi_driver_t *instance); /**< 进入低功耗状态。 */
    temp_humi_status_t (*pf_wakeup)(bsp_temp_humi_driver_t *instance); /**< 唤醒并恢复测量能力。 */
};

/**
 * @brief 装配并初始化 SHT30 实例。
 * @param instance 待填充的通用 BSP 驱动实例。
 * @param iic Port 提供的 I2C 原语及互斥接口。
 * @param yield Port 提供的 OSAL 延时适配接口。
 * @return TEMP_HUMI_OK 表示实例可用，其余值表示初始化失败原因。
 */
temp_humi_status_t sht30_inst(bsp_temp_humi_driver_t *instance,
                              temp_humi_iic_interface_t *iic,
                              temp_humi_yield_interface_t *yield);

/* The handler calls only this model-independent assembly entry. */
#define bsp_temp_humi_inst sht30_inst

#endif /* BSP_SHT30_DRIVER_H */
