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

/** @brief 触摸 BSP 统一返回状态。 */
typedef enum
{
    TOUCH_OK = 0,          /**< 操作成功并返回有效事件/触点。 */
    TOUCH_ERROR,           /**< 通信或触摸数据错误。 */
    TOUCH_ERROR_PARAMETER, /**< 空指针或配置参数非法。 */
    TOUCH_ERROR_RESOURCE,  /**< 驱动未初始化或资源不可用。 */
    TOUCH_NO_TOUCH,        /**< 当前没有有效触点，不属于错误。 */
    TOUCH_ERROR_TIMEOUT    /**< 总线应答或互斥锁等待超时。 */
} touch_status_t;

/** @brief Handler 对外生成的控制器无关触点生命周期事件。 */
typedef enum
{
    TOUCH_EVENT_NONE = 0, /**< 没有可发布事件。 */
    TOUCH_EVENT_DOWN,     /**< 新触点已确认按下。 */
    TOUCH_EVENT_MOVE,     /**< 触点保持或发生移动。 */
    TOUCH_EVENT_UP        /**< 已确认释放最后一个触点。 */
} touch_event_t;

/**
 * @brief 触摸控制器使用的 I2C 抽象接口。
 * @note pf_lock/pf_unlock 保护一次完整寄存器事务，必须成对实现或同时为空。
 */
typedef struct
{
    void *bus_context; /**< Port 持有并原样回传的总线上下文。 */
    touch_status_t (*pf_iic_init)(void *bus_context); /**< 初始化 I2C 总线。 */
    touch_status_t (*pf_iic_deinit)(void *bus_context); /**< 可选的反初始化。 */
    touch_status_t (*pf_iic_start)(void *bus_context); /**< 产生 START 条件。 */
    touch_status_t (*pf_iic_stop)(void *bus_context); /**< 产生 STOP 条件。 */
    touch_status_t (*pf_iic_wait_ack)(void *bus_context); /**< 等待从机 ACK。 */
    touch_status_t (*pf_iic_send_ack)(void *bus_context); /**< 主机发送 ACK。 */
    touch_status_t (*pf_iic_send_no_ack)(void *bus_context); /**< 主机发送 NACK。 */
    touch_status_t (*pf_iic_send_byte)(void *bus_context, uint8_t data); /**< 发送一字节。 */
    touch_status_t (*pf_iic_receive_byte)(void *bus_context, uint8_t *data); /**< 接收一字节。 */
    touch_status_t (*pf_lock)(void *bus_context, uint32_t timeout_ms); /**< 锁定完整事务。 */
    touch_status_t (*pf_unlock)(void *bus_context); /**< 释放事务锁。 */
} touch_iic_interface_t;

/** @brief BSP 所需的阻塞让出接口，由 Port 映射到 OSAL 延时。 */
typedef struct
{
    void (*pf_rtos_yield)(uint32_t milliseconds); /**< 至少让出指定毫秒数。 */
} touch_yield_interface_t;

/** @brief 触摸芯片复位和可选中断状态接口。 */
typedef struct
{
    void *context; /**< Port 持有并回传的 GPIO 上下文。 */
    void (*pf_set_reset)(void *context, bool high); /**< 设置复位引脚电平。 */
    bool (*pf_is_interrupt_asserted)(void *context); /**< 查询中断有效电平；允许为空。 */
} touch_control_interface_t;

/** @brief 单个触点及其 Handler 发布元数据。 */
typedef struct
{
    uint16_t x; /**< 原始或映射后的 X 坐标，取决于所处层级。 */
    uint16_t y; /**< 原始或映射后的 Y 坐标，取决于所处层级。 */
    uint8_t gesture; /**< 控制器报告的手势标识；0 表示无手势。 */
    uint8_t event; /**< 驱动中为芯片原始事件，Handler 输出 touch_event_t。 */
    uint8_t fingers; /**< 当前有效触点数量；CST816T 最大为 1。 */
    uint32_t timestamp_ms; /**< Handler 采样时的单调时钟毫秒值。 */
    uint32_t sequence; /**< 每次发布事件递增的序号。 */
} touch_point_t;

/** @brief 触摸设备对上层报告的逻辑能力。 */
typedef struct
{
    uint16_t width; /**< 当前输出坐标宽度。 */
    uint16_t height; /**< 当前输出坐标高度。 */
    uint8_t max_points; /**< 硬件可同时报告的最大触点数。 */
} touch_info_t;

/** @brief 允许驱动操作表回调引用自身实例的前向类型。 */
typedef struct bsp_touch_driver bsp_touch_driver_t;

/** @brief BSP 触摸控制器实例及型号无关操作表。 */
struct bsp_touch_driver
{
    const touch_iic_interface_t *p_iic_driver_instance; /**< Port 注入的 I2C 接口。 */
    const touch_yield_interface_t *p_yield_instance; /**< Port 注入的 OS 让出接口。 */
    const touch_control_interface_t *p_control_instance; /**< Port 注入的 GPIO 接口。 */
    uint16_t width; /**< 芯片原始坐标宽度。 */
    uint16_t height; /**< 芯片原始坐标高度。 */
    uint8_t chip_id; /**< 初始化时读取的芯片标识。 */
    uint8_t firmware_version; /**< 初始化时读取的固件版本。 */
    bool initialized; /**< 控制器初始化成功标志。 */

    touch_status_t (*pf_init)(bsp_touch_driver_t *touch); /**< 复位、探测并初始化设备。 */
    touch_status_t (*pf_read_point)(bsp_touch_driver_t *touch,
                                    touch_point_t *point); /**< 读取一个原始触点。 */
    touch_status_t (*pf_get_info)(const bsp_touch_driver_t *touch,
                                  touch_info_t *info); /**< 获取原始坐标能力。 */
    touch_status_t (*pf_sleep)(bsp_touch_driver_t *touch); /**< 进入低功耗状态。 */
    touch_status_t (*pf_wakeup)(bsp_touch_driver_t *touch); /**< 唤醒并重新初始化。 */
};

/** @brief 装配并初始化 CST816T 通用触摸实例。 */
touch_status_t cst816t_inst(bsp_touch_driver_t *touch,
                            const touch_iic_interface_t *iic,
                            const touch_yield_interface_t *yield,
                            const touch_control_interface_t *control);

#define bsp_touch_inst cst816t_inst

#endif /* BSP_CST816T_DRIVER_H */
