#ifndef BSP_SPIFLASH_DRIVER_H
#define BSP_SPIFLASH_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief SPI Flash BSP 返回状态。 */
typedef enum {
    SPIFLASH_OK = 0,              /**< 操作成功。 */
    SPIFLASH_ERROR_PARAMETER,     /**< 空指针、零长度或参数非法。 */
    SPIFLASH_ERROR_NOT_READY,     /**< SFUD 设备尚未初始化。 */
    SPIFLASH_ERROR_NOT_FOUND,     /**< 未探测到匹配的 SPI Flash。 */
    SPIFLASH_ERROR_OUT_OF_RANGE,  /**< 地址区间超出物理 Flash 容量。 */
    SPIFLASH_ERROR_TIMEOUT,       /**< SPI 传输或总线互斥等待超时。 */
    SPIFLASH_ERROR_IO,            /**< SFUD/SPI 擦写或读取失败。 */
} spiflash_status_t;

/** @brief 由 SFUD 探测得到的物理 Flash 能力。 */
typedef struct {
    uint32_t capacity; /**< 总容量，单位字节。 */
    uint32_t erase_size; /**< 最小擦除单元，单位字节。 */
    uint32_t write_granularity_bits; /**< 最小可编程粒度，单位 bit。 */
} spiflash_info_t;

/**
 * @brief SFUD 所需的 SPI 总线抽象接口。
 * @note pf_lock/pf_unlock 保护一次完整的命令、地址及数据传输。
 */
typedef struct {
    void *bus_context; /**< Port 持有并回传的 SPI/片选上下文。 */
    spiflash_status_t (*pf_spi_init)(void *context); /**< 初始化 SPI 和片选 GPIO。 */
    spiflash_status_t (*pf_write_read)(void *context,
                                       const uint8_t *write_buffer,
                                       size_t write_size,
                                       uint8_t *read_buffer,
                                       size_t read_size); /**< 执行保持片选有效的半双工事务。 */
    spiflash_status_t (*pf_lock)(void *context, uint32_t timeout_ms); /**< 锁定总线事务。 */
    spiflash_status_t (*pf_unlock)(void *context); /**< 释放总线事务锁。 */
} spiflash_spi_driver_interface_t;

/** @brief SFUD 忙等待期间使用的 OS 让出接口。 */
typedef struct {
    void (*pf_rtos_yield)(uint32_t milliseconds); /**< 至少让出指定毫秒数。 */
} spiflash_yield_interface_t;

/** @brief SPI Flash BSP 实例，封装 SFUD 设备及注入接口。 */
typedef struct {
    void *device; /**< SFUD 设备指针；保持 void * 以隔离第三方类型。 */
    spiflash_spi_driver_interface_t spi; /**< 注册时复制的 SPI 接口。 */
    spiflash_yield_interface_t yield; /**< 注册时复制的 OS 让出接口。 */
    bool lock_failed; /**< SFUD 锁回调是否曾获取失败。 */
} bsp_spiflash_driver_t;

/** @brief 装配接口、初始化 SFUD 并探测当前 SPI Flash。 */
spiflash_status_t spiflash_inst(
    bsp_spiflash_driver_t *driver,
    const spiflash_spi_driver_interface_t *spi,
    const spiflash_yield_interface_t *yield);
/** @brief 查询实例是否已绑定可用的 SFUD 设备。 */
bool spiflash_is_ready(const bsp_spiflash_driver_t *driver);
/** @brief 读取物理容量、擦除粒度和写粒度。 */
spiflash_status_t spiflash_get_info(bsp_spiflash_driver_t *driver,
                                    spiflash_info_t *info);
/** @brief 从绝对物理地址读取数据；函数会校验容量边界。 */
spiflash_status_t spiflash_read(bsp_spiflash_driver_t *driver,
                                uint32_t address, void *buffer, size_t size);
/** @brief 向绝对物理地址编程数据；目标区域应预先处于擦除态。 */
spiflash_status_t spiflash_write(bsp_spiflash_driver_t *driver,
                                 uint32_t address, const void *buffer,
                                 size_t size);
/** @brief 擦除绝对物理地址范围；地址和长度必须满足擦除粒度。 */
spiflash_status_t spiflash_erase(bsp_spiflash_driver_t *driver,
                                 uint32_t address, size_t size);

#endif /* BSP_SPIFLASH_DRIVER_H */
