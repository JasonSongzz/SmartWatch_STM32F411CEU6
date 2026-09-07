#ifndef DRV_ADAPTER_FLASH_H
#define DRV_ADAPTER_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLASH_DEV_EXTERNAL (0U)
#define FLASH_DEV_MAX      (2U)
#define FLASH_CONFIG_MAX_DATA_SIZE 1024U
#define FLASH_LOG_MAX_DATA_SIZE     512U

typedef int64_t flash_log_time_t;

/** @brief Flash Wrapper 及 FlashDB 业务接口统一状态码。 */
typedef enum {
    FLASH_STATUS_OK = 0,              /**< 操作成功。 */
    FLASH_STATUS_DEGRADED = 1,        /**< 操作成功，但双备份中仅一份有效。 */
    FLASH_STATUS_ERROR_PARAM = -1,    /**< 空指针、零长度或非法枚举。 */
    FLASH_STATUS_NOT_READY = -2,      /**< 物理 Flash 或数据库尚未初始化。 */
    FLASH_STATUS_NOT_FOUND = -3,      /**< 配置键或请求记录不存在。 */
    FLASH_STATUS_NO_SPACE = -4,       /**< 分区或数据库剩余空间不足。 */
    FLASH_STATUS_IO_ERROR = -5,       /**< SPI、SFUD、FAL 或 FlashDB 操作失败。 */
    FLASH_STATUS_OUT_OF_RANGE = -6,   /**< 请求范围超出设备或分区。 */
    FLASH_STATUS_ALIGNMENT_ERROR = -7 /**< 擦除范围未按扇区对齐。 */
} flash_status_t;

/**
 * @brief 日志遍历访问器。
 * @return true 立即停止遍历，false 继续。
 * @note data 只在回调执行期间有效；回调执行时不持有 FlashDB/API 锁，
 *       可调用其他 Flash API，但不得递归启动日志遍历。
 */
typedef bool (*flash_log_visitor_t)(flash_log_time_t timestamp,
                                    const void *data,
                                    size_t size,
                                    void *argument);

/** @brief 上层可访问的逻辑 Flash 分区标识。 */
typedef enum {
    FLASH_REGION_OTA = 0,       /**< OTA 固件备份区。 */
    FLASH_REGION_CONFIG_MAIN,   /**< FlashDB 配置主区。 */
    FLASH_REGION_CONFIG_BACKUP, /**< FlashDB 配置备份区。 */
    FLASH_REGION_LOG,           /**< FlashDB TSDB 日志区。 */
    FLASH_REGION_IMAGE,         /**< 图片等原始资源区。 */
    FLASH_REGION_MAX,           /**< 分区枚举数量，不是有效分区。 */
} flash_region_t;

/** @brief 物理 SPI Flash 的容量和擦写能力。 */
typedef struct {
    uint32_t capacity; /**< 总容量，字节。 */
    uint32_t erase_size; /**< 最小擦除单元，字节。 */
    uint32_t write_granularity_bits; /**< 最小编程粒度，bit。 */
} flash_dev_info_t;

/** @brief 单个逻辑分区的绝对布局信息。 */
typedef struct {
    const char *name; /**< 静态分区名称，调用方不得修改或释放。 */
    uint32_t offset; /**< 相对物理 Flash 起点的绝对偏移，字节。 */
    uint32_t size; /**< 分区容量，字节。 */
    uint32_t erase_size; /**< 该分区要求的擦除对齐，字节。 */
} flash_region_info_t;

/**
 * @brief Wrapper 保存的 Flash 设备操作表。
 *
 * raw read/write/erase 使用物理绝对地址；region API 在 Wrapper 中完成
 * 分区边界换算；config/log 回调由 Port 内的 FlashDB 后端实现。
 */
typedef struct flash_drv {
    uint32_t idx; /**< Wrapper 写入的设备索引。 */
    void *user_data; /**< Port 私有上下文，Wrapper 不解析。 */
    bool (*init)(struct flash_drv *dev); /**< 初始化 SFUD、FAL 和 FlashDB。 */
    bool (*get_info)(struct flash_drv *dev, flash_dev_info_t *info); /**< 获取物理能力。 */
    bool (*get_region_info)(struct flash_drv *dev, flash_region_t region,
                            flash_region_info_t *info); /**< 获取逻辑分区布局。 */
    bool (*read)(struct flash_drv *dev, uint32_t address,
                 void *buffer, size_t size); /**< 绝对地址原始读取。 */
    bool (*write)(struct flash_drv *dev, uint32_t address,
                  const void *buffer, size_t size); /**< 绝对地址原始编程。 */
    bool (*erase)(struct flash_drv *dev, uint32_t address, size_t size); /**< 绝对地址原始擦除。 */
    flash_status_t (*config_save)(struct flash_drv *dev, const char *key,
                                  const void *data, size_t size); /**< 双备份保存配置值。 */
    flash_status_t (*config_load)(struct flash_drv *dev, const char *key,
                                  void *data, size_t *size); /**< 加载最新有效配置值。 */
    flash_status_t (*config_delete)(struct flash_drv *dev, const char *key); /**< 删除双区配置键。 */
    flash_status_t (*log_append)(struct flash_drv *dev,
                                 flash_log_time_t timestamp,
                                 const void *data, size_t size); /**< 追加一条 TSDB 日志。 */
    size_t (*log_visit_latest)(struct flash_drv *dev, size_t max_entries,
                               flash_log_visitor_t visitor, void *argument); /**< 从最新记录开始遍历。 */
    flash_status_t (*log_clear)(struct flash_drv *dev); /**< 清空日志数据库。 */
} flash_drv_t;

/** @brief 注册 Flash 设备操作表；只复制结构，不访问硬件。 */
bool drv_adapter_flash_reg(uint32_t index, const flash_drv_t *driver);
/** @brief 初始化指定 Flash 设备及其数据库后端。 */
bool drv_adapter_flash_init(uint32_t index);
/** @brief 获取物理 Flash 能力。 */
bool drv_adapter_flash_get_info(uint32_t index, flash_dev_info_t *info);
/** @brief 获取指定逻辑分区布局。 */
bool drv_adapter_flash_region_get_info(uint32_t index, flash_region_t region,
                                       flash_region_info_t *info);
/** @brief 按绝对物理地址读取原始数据。 */
bool drv_adapter_flash_read(uint32_t index, uint32_t address,
                            void *buffer, size_t size);
/** @brief 按绝对物理地址编程原始数据；不会自动擦除。 */
bool drv_adapter_flash_write(uint32_t index, uint32_t address,
                             const void *buffer, size_t size);
/** @brief 按绝对物理地址擦除；必须满足设备擦除对齐。 */
bool drv_adapter_flash_erase(uint32_t index, uint32_t address, size_t size);
/** @brief 以分区内相对偏移读取数据并校验边界。 */
flash_status_t drv_adapter_flash_region_read(uint32_t index,
                                             flash_region_t region,
                                             uint32_t offset, void *buffer,
                                             size_t size);
/** @brief 以分区内相对偏移编程数据并校验边界；不会自动擦除。 */
flash_status_t drv_adapter_flash_region_write(uint32_t index,
                                              flash_region_t region,
                                              uint32_t offset,
                                              const void *buffer,
                                              size_t size);
/** @brief 擦除分区内相对范围并校验边界和对齐。 */
flash_status_t drv_adapter_flash_region_erase(uint32_t index,
                                              flash_region_t region,
                                              uint32_t offset, size_t size);
/** @brief 通过 FlashDB 将配置值写入主、备两个配置区。 */
flash_status_t drv_adapter_flash_config_save(uint32_t index, const char *key,
                                             const void *data, size_t size);
/** @brief 加载序号最新且 CRC 有效的配置值，并可报告降级状态。 */
flash_status_t drv_adapter_flash_config_load(uint32_t index, const char *key,
                                             void *data, size_t *size);
/** @brief 从主、备两个配置数据库删除指定键。 */
flash_status_t drv_adapter_flash_config_delete(uint32_t index,
                                               const char *key);
/** @brief 向日志 TSDB 追加一条记录。 */
flash_status_t drv_adapter_flash_log_append(uint32_t index,
                                            flash_log_time_t timestamp,
                                            const void *data, size_t size);
/** @brief 从最新日志开始回调访问，返回实际访问条数。 */
size_t drv_adapter_flash_log_visit_latest(uint32_t index, size_t max_entries,
                                          flash_log_visitor_t visitor,
                                          void *argument);
/** @brief 清空日志 TSDB。 */
flash_status_t drv_adapter_flash_log_clear(uint32_t index);

#endif /* DRV_ADAPTER_FLASH_H */
