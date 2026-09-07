#ifndef BSP_MPU6050_DRIVER_H
#define BSP_MPU6050_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

/* MPU6050 protocol configuration. */
#define MPU6050_I2C_ADDRESS_AD0_LOW   0x68U
#define MPU6050_I2C_ADDRESS_AD0_HIGH  0x69U
#define MPU6050_I2C_ADDRESS           MPU6050_I2C_ADDRESS_AD0_LOW
#define MPU6050_REG_SELF_TEST_X       0x0DU
#define MPU6050_REG_SMPLRT_DIV        0x19U
#define MPU6050_REG_CONFIG            0x1AU
#define MPU6050_REG_GYRO_CONFIG       0x1BU
#define MPU6050_REG_ACCEL_CONFIG      0x1CU
#define MPU6050_REG_ACCEL_XOUT_H      0x3BU
#define MPU6050_REG_TEMP_OUT_H        0x41U
#define MPU6050_REG_GYRO_XOUT_H       0x43U
#define MPU6050_REG_PWR_MGMT_1        0x6BU
#define MPU6050_REG_PWR_MGMT_2        0x6CU
#define MPU6050_REG_WHO_AM_I          0x75U
#define MPU6050_PWR1_DEVICE_RESET     0x80U
#define MPU6050_PWR1_SLEEP            0x40U
#define MPU6050_PWR1_CLKSEL_PLL_XGYRO 0x01U
#define MPU6050_WHO_AM_I_MASK         0x7EU
#define MPU6050_ACCEL_FS_2G           0x00U
#define MPU6050_ACCEL_FS_4G           0x08U
#define MPU6050_ACCEL_FS_8G           0x10U
#define MPU6050_ACCEL_FS_16G          0x18U
#define MPU6050_ACCEL_SENSITIVITY_2G  16384.0f
#define MPU6050_ACCEL_SENSITIVITY_4G  8192.0f
#define MPU6050_ACCEL_SENSITIVITY_8G  4096.0f
#define MPU6050_ACCEL_SENSITIVITY_16G 2048.0f
#define MPU6050_GYRO_FS_250DPS          0x00U
#define MPU6050_GYRO_FS_500DPS          0x08U
#define MPU6050_GYRO_FS_1000DPS         0x10U
#define MPU6050_GYRO_FS_2000DPS         0x18U
#define MPU6050_GYRO_SENSITIVITY_250DPS  131.0f
#define MPU6050_GYRO_SENSITIVITY_500DPS  65.5f
#define MPU6050_GYRO_SENSITIVITY_1000DPS 32.8f
#define MPU6050_GYRO_SENSITIVITY_2000DPS 16.4f
#define MPU6050_RESET_WAIT_MS         100U
#define MPU6050_WAKEUP_WAIT_MS        10U
#define MPU6050_DATA_LENGTH           6U
#define MPU6050_IMU_FRAME_LENGTH      14U

/** @brief 加速度计 BSP 统一返回状态。 */
typedef enum
{
    ACCEL_OK = 0,             /**< 操作成功。 */
    ACCEL_ERROR,              /**< 设备通信或数据错误。 */
    ACCEL_ERROR_TIMEOUT,      /**< 总线应答或互斥锁等待超时。 */
    ACCEL_ERROR_RESOURCE,     /**< 驱动未初始化或所需资源不可用。 */
    ACCEL_ERROR_PARAMETER,    /**< 传入了空指针或非法参数。 */
    ACCEL_ERROR_ID            /**< WHO_AM_I 与目标器件不匹配。 */
} accel_status_t;

/**
 * @brief 加速度计驱动使用的 I2C 抽象接口。
 *
 * BSP 只描述一次完整 I2C 事务需要的原语，不依赖 STM32 HAL 或具体 RTOS。
 * pf_lock/pf_unlock 保护的是一整段总线事务，两者必须同时提供或同时为空。
 */
typedef struct
{
    void *bus_context; /**< 由 Port 持有并原样回传的总线上下文。 */
    accel_status_t (*pf_iic_init)(void *bus_context); /**< 初始化 I2C 总线。 */
    accel_status_t (*pf_iic_deinit)(void *bus_context); /**< 可选的反初始化。 */
    accel_status_t (*pf_iic_start)(void *bus_context); /**< 产生 START 条件。 */
    accel_status_t (*pf_iic_stop)(void *bus_context); /**< 产生 STOP 条件。 */
    accel_status_t (*pf_iic_wait_ack)(void *bus_context); /**< 等待从机 ACK。 */
    accel_status_t (*pf_iic_send_ack)(void *bus_context); /**< 主机发送 ACK。 */
    accel_status_t (*pf_iic_send_no_ack)(void *bus_context); /**< 主机发送 NACK。 */
    accel_status_t (*pf_iic_send_byte)(void *bus_context, uint8_t data); /**< 发送一字节。 */
    accel_status_t (*pf_iic_receive_byte)(void *bus_context, uint8_t *data); /**< 接收一字节。 */
    accel_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms); /**< 锁定整条总线事务。 */
    accel_status_t (*pf_unlock)(void *bus_context); /**< 释放总线事务锁。 */
} accel_iic_interface_t;

/** @brief BSP 所需的阻塞让出接口，由 Port 映射到 OSAL 延时。 */
typedef struct
{
    void (*pf_rtos_yield)(uint32_t milliseconds); /**< 至少让出指定毫秒数。 */
} accel_yield_interface_t;

/** @brief 传感器原始三轴有符号采样值，单位为 LSB。 */
typedef struct
{
    int16_t x; /**< X 轴原始值，LSB。 */
    int16_t y; /**< Y 轴原始值，LSB。 */
    int16_t z; /**< Z 轴原始值，LSB。 */
} accel_raw_data_t;

/** @brief 通用三轴浮点向量，具体单位由使用它的字段决定。 */
typedef struct
{
    float x; /**< X 轴物理量。 */
    float y; /**< Y 轴物理量。 */
    float z; /**< Z 轴物理量。 */
} accel_data_t;

/** @brief MPU6050 一次突发读取获得的原始完整 IMU 帧。 */
typedef struct
{
    accel_raw_data_t accel; /**< 三轴加速度原始值，LSB。 */
    int16_t temperature;    /**< 芯片温度原始值，LSB。 */
    accel_raw_data_t gyro;  /**< 三轴角速度原始值，LSB。 */
} accel_raw_imu_data_t;

/** @brief 已按量程灵敏度换算的完整 IMU 数据。 */
typedef struct
{
    accel_data_t accel_g; /**< 三轴加速度，单位 g。 */
    accel_data_t gyro_dps; /**< 三轴角速度，单位 degree/s。 */
    float temperature_c; /**< 芯片温度，单位摄氏度。 */
} accel_imu_data_t;

/** @brief 允许驱动操作表回调引用自身实例的前向类型。 */
typedef struct bsp_accel_driver bsp_accel_driver_t;

/**
 * @brief BSP 加速度计实例及型号无关操作表。
 *
 * Handler 只依赖此结构；更换器件时，新驱动应填充相同字段和回调语义。
 */
struct bsp_accel_driver
{
    accel_iic_interface_t *p_iic_driver_instance; /**< Port 注入的 I2C 接口。 */
    accel_yield_interface_t *p_yield_instance; /**< Port 注入的 OS 让出接口。 */
    bool is_inited; /**< 硬件初始化成功标志。 */
    uint8_t accel_config; /**< 当前加速度量程寄存器配置。 */
    uint8_t gyro_config; /**< 当前陀螺仪量程寄存器配置。 */
    float accel_sensitivity; /**< 加速度灵敏度，LSB/g。 */
    float gyro_sensitivity; /**< 角速度灵敏度，LSB/(degree/s)。 */

    accel_status_t (*pf_init)(bsp_accel_driver_t *instance); /**< 初始化并校验器件。 */
    accel_status_t (*pf_deinit)(bsp_accel_driver_t *instance); /**< 释放器件资源。 */
    accel_status_t (*pf_read_id)(bsp_accel_driver_t *instance, uint8_t *id); /**< 读取器件标识。 */
    accel_status_t (*pf_read_raw_accel)(bsp_accel_driver_t *instance,
                                        accel_raw_data_t *accel); /**< 读取原始加速度。 */
    accel_status_t (*pf_read_accel)(bsp_accel_driver_t *instance,
                                    accel_data_t *accel); /**< 读取以 g 表示的加速度。 */
    accel_status_t (*pf_read_imu)(bsp_accel_driver_t *instance,
                                  accel_imu_data_t *imu); /**< 原子读取完整 IMU 帧。 */
    accel_status_t (*pf_sleep)(bsp_accel_driver_t *instance); /**< 进入低功耗状态。 */
    accel_status_t (*pf_wakeup)(bsp_accel_driver_t *instance); /**< 唤醒并恢复工作配置。 */
};

/**
 * @brief 装配并初始化 MPU6050 实例。
 * @param instance 待填充的通用 BSP 驱动实例。
 * @param iic Port 提供的 I2C 原语及互斥接口。
 * @param yield Port 提供的 OSAL 延时适配接口。
 * @return ACCEL_OK 表示实例可用，其余值表示初始化失败原因。
 */
accel_status_t mpu6050_inst(bsp_accel_driver_t *instance,
                            accel_iic_interface_t *iic,
                            accel_yield_interface_t *yield);

#define bsp_accel_inst mpu6050_inst

#endif /* BSP_MPU6050_DRIVER_H */
