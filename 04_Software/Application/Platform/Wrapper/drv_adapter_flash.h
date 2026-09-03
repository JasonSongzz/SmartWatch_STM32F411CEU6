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

typedef enum {
    FLASH_STATUS_OK = 0,
    FLASH_STATUS_DEGRADED = 1,
    FLASH_STATUS_ERROR_PARAM = -1,
    FLASH_STATUS_NOT_READY = -2,
    FLASH_STATUS_NOT_FOUND = -3,
    FLASH_STATUS_NO_SPACE = -4,
    FLASH_STATUS_IO_ERROR = -5,
    FLASH_STATUS_OUT_OF_RANGE = -6,
    FLASH_STATUS_ALIGNMENT_ERROR = -7,
} flash_status_t;

/* Returning true stops iteration. Data is valid only during the callback. */
typedef bool (*flash_log_visitor_t)(flash_log_time_t timestamp,
                                    const void *data,
                                    size_t size,
                                    void *argument);

typedef enum {
    FLASH_REGION_OTA = 0,
    FLASH_REGION_CONFIG_MAIN,
    FLASH_REGION_CONFIG_BACKUP,
    FLASH_REGION_LOG,
    FLASH_REGION_IMAGE,
    FLASH_REGION_MAX,
} flash_region_t;

typedef struct {
    uint32_t capacity;
    uint32_t erase_size;
    uint32_t write_granularity_bits;
} flash_dev_info_t;

typedef struct {
    const char *name;
    uint32_t offset;
    uint32_t size;
    uint32_t erase_size;
} flash_region_info_t;

typedef struct flash_drv {
    uint32_t idx;
    void *user_data;
    bool (*init)(struct flash_drv *dev);
    bool (*get_info)(struct flash_drv *dev, flash_dev_info_t *info);
    bool (*get_region_info)(struct flash_drv *dev, flash_region_t region,
                            flash_region_info_t *info);
    bool (*read)(struct flash_drv *dev, uint32_t address,
                 void *buffer, size_t size);
    bool (*write)(struct flash_drv *dev, uint32_t address,
                  const void *buffer, size_t size);
    bool (*erase)(struct flash_drv *dev, uint32_t address, size_t size);
    flash_status_t (*config_save)(struct flash_drv *dev, const char *key,
                                  const void *data, size_t size);
    flash_status_t (*config_load)(struct flash_drv *dev, const char *key,
                                  void *data, size_t *size);
    flash_status_t (*config_delete)(struct flash_drv *dev, const char *key);
    flash_status_t (*log_append)(struct flash_drv *dev,
                                 flash_log_time_t timestamp,
                                 const void *data, size_t size);
    size_t (*log_visit_latest)(struct flash_drv *dev, size_t max_entries,
                               flash_log_visitor_t visitor, void *argument);
    flash_status_t (*log_clear)(struct flash_drv *dev);
} flash_drv_t;

bool drv_adapter_flash_reg(uint32_t index, const flash_drv_t *driver);
bool drv_adapter_flash_init(uint32_t index);
bool drv_adapter_flash_get_info(uint32_t index, flash_dev_info_t *info);
bool drv_adapter_flash_region_get_info(uint32_t index, flash_region_t region,
                                       flash_region_info_t *info);
bool drv_adapter_flash_read(uint32_t index, uint32_t address,
                            void *buffer, size_t size);
bool drv_adapter_flash_write(uint32_t index, uint32_t address,
                             const void *buffer, size_t size);
bool drv_adapter_flash_erase(uint32_t index, uint32_t address, size_t size);
flash_status_t drv_adapter_flash_region_read(uint32_t index,
                                             flash_region_t region,
                                             uint32_t offset, void *buffer,
                                             size_t size);
flash_status_t drv_adapter_flash_region_write(uint32_t index,
                                              flash_region_t region,
                                              uint32_t offset,
                                              const void *buffer,
                                              size_t size);
flash_status_t drv_adapter_flash_region_erase(uint32_t index,
                                              flash_region_t region,
                                              uint32_t offset, size_t size);
flash_status_t drv_adapter_flash_config_save(uint32_t index, const char *key,
                                             const void *data, size_t size);
flash_status_t drv_adapter_flash_config_load(uint32_t index, const char *key,
                                             void *data, size_t *size);
flash_status_t drv_adapter_flash_config_delete(uint32_t index,
                                               const char *key);
flash_status_t drv_adapter_flash_log_append(uint32_t index,
                                            flash_log_time_t timestamp,
                                            const void *data, size_t size);
size_t drv_adapter_flash_log_visit_latest(uint32_t index, size_t max_entries,
                                          flash_log_visitor_t visitor,
                                          void *argument);
flash_status_t drv_adapter_flash_log_clear(uint32_t index);

#endif /* DRV_ADAPTER_FLASH_H */
