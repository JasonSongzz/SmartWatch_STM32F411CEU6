#ifndef STORAGE_SERVICE_H
#define STORAGE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STORAGE_CONFIG_MAX_DATA_SIZE 1024U
#define STORAGE_LOG_MAX_DATA_SIZE     512U

typedef int64_t storage_log_time_t;

typedef enum {
    STORAGE_STATUS_OK = 0,
    STORAGE_STATUS_DEGRADED = 1,
    STORAGE_STATUS_ERROR_PARAM = -1,
    STORAGE_STATUS_NOT_READY = -2,
    STORAGE_STATUS_NOT_FOUND = -3,
    STORAGE_STATUS_NO_SPACE = -4,
    STORAGE_STATUS_IO_ERROR = -5,
    STORAGE_STATUS_OUT_OF_RANGE = -6,
    STORAGE_STATUS_ALIGNMENT_ERROR = -7,
} storage_status_t;

typedef struct {
    uint32_t capacity;
    uint32_t erase_size;
} storage_area_info_t;

/* Returning true stops iteration. Data is callback-only; do not re-enter this service. */
typedef bool (*storage_log_visitor_t)(storage_log_time_t timestamp,
                                      const void *data,
                                      size_t size,
                                      void *argument);

storage_status_t storage_service_init(void);
bool storage_service_is_ready(void);

storage_status_t storage_config_save(const char *key, const void *data, size_t size);
storage_status_t storage_config_load(const char *key, void *data, size_t *size);
storage_status_t storage_config_delete(const char *key);

/* A timestamp <= the latest stored timestamp is normalized to latest + 1. */
storage_status_t storage_log_append(storage_log_time_t timestamp,
                                    const void *data,
                                    size_t size);
size_t storage_log_visit_latest(size_t max_entries,
                                storage_log_visitor_t visitor,
                                void *argument);
storage_status_t storage_log_clear(void);

/* Raw image/OTA writes require the target bytes to be erased beforehand. */
storage_status_t storage_image_get_info(storage_area_info_t *info);
storage_status_t storage_image_read(uint32_t offset, void *data, size_t size);
storage_status_t storage_image_write(uint32_t offset, const void *data,
                                     size_t size);
storage_status_t storage_image_erase(uint32_t offset, size_t size);

storage_status_t storage_ota_get_info(storage_area_info_t *info);
storage_status_t storage_ota_read(uint32_t offset, void *data, size_t size);
storage_status_t storage_ota_write(uint32_t offset, const void *data,
                                   size_t size);
storage_status_t storage_ota_erase(uint32_t offset, size_t size);

#endif /* STORAGE_SERVICE_H */
