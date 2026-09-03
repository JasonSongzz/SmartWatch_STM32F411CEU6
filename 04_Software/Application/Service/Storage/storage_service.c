#include "storage_service.h"

#include "drv_adapter_flash.h"

_Static_assert(STORAGE_CONFIG_MAX_DATA_SIZE == FLASH_CONFIG_MAX_DATA_SIZE,
               "Storage and flash config limits must match");
_Static_assert(STORAGE_LOG_MAX_DATA_SIZE == FLASH_LOG_MAX_DATA_SIZE,
               "Storage and flash log limits must match");

static bool s_storage_ready;

static storage_status_t storage_status_from_flash(flash_status_t status)
{
    switch (status) {
    case FLASH_STATUS_OK:
        return STORAGE_STATUS_OK;
    case FLASH_STATUS_DEGRADED:
        return STORAGE_STATUS_DEGRADED;
    case FLASH_STATUS_ERROR_PARAM:
        return STORAGE_STATUS_ERROR_PARAM;
    case FLASH_STATUS_NOT_READY:
        return STORAGE_STATUS_NOT_READY;
    case FLASH_STATUS_NOT_FOUND:
        return STORAGE_STATUS_NOT_FOUND;
    case FLASH_STATUS_NO_SPACE:
        return STORAGE_STATUS_NO_SPACE;
    case FLASH_STATUS_OUT_OF_RANGE:
        return STORAGE_STATUS_OUT_OF_RANGE;
    case FLASH_STATUS_ALIGNMENT_ERROR:
        return STORAGE_STATUS_ALIGNMENT_ERROR;
    case FLASH_STATUS_IO_ERROR:
    default:
        return STORAGE_STATUS_IO_ERROR;
    }
}

static storage_status_t storage_area_get_info(flash_region_t region,
                                              storage_area_info_t *info)
{
    flash_region_info_t region_info;

    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    if (info == NULL) return STORAGE_STATUS_ERROR_PARAM;
    if (!drv_adapter_flash_region_get_info(FLASH_DEV_EXTERNAL, region,
                                           &region_info)) {
        return STORAGE_STATUS_IO_ERROR;
    }

    info->capacity = region_info.size;
    info->erase_size = region_info.erase_size;
    return STORAGE_STATUS_OK;
}

static storage_status_t storage_area_read(flash_region_t region,
                                          uint32_t offset,
                                          void *data,
                                          size_t size)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    return storage_status_from_flash(drv_adapter_flash_region_read(
        FLASH_DEV_EXTERNAL, region, offset, data, size));
}

static storage_status_t storage_area_write(flash_region_t region,
                                           uint32_t offset,
                                           const void *data,
                                           size_t size)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    return storage_status_from_flash(drv_adapter_flash_region_write(
        FLASH_DEV_EXTERNAL, region, offset, data, size));
}

static storage_status_t storage_area_erase(flash_region_t region,
                                           uint32_t offset,
                                           size_t size)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    return storage_status_from_flash(drv_adapter_flash_region_erase(
        FLASH_DEV_EXTERNAL, region, offset, size));
}

storage_status_t storage_service_init(void)
{
    if (s_storage_ready) return STORAGE_STATUS_OK;
    if (!drv_adapter_flash_init(FLASH_DEV_EXTERNAL))
        return STORAGE_STATUS_IO_ERROR;

    s_storage_ready = true;
    return STORAGE_STATUS_OK;
}

bool storage_service_is_ready(void)
{
    return s_storage_ready;
}

storage_status_t storage_config_save(const char *key, const void *data,
                                     size_t size)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    return storage_status_from_flash(drv_adapter_flash_config_save(
        FLASH_DEV_EXTERNAL, key, data, size));
}

storage_status_t storage_config_load(const char *key, void *data, size_t *size)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    return storage_status_from_flash(drv_adapter_flash_config_load(
        FLASH_DEV_EXTERNAL, key, data, size));
}

storage_status_t storage_config_delete(const char *key)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    return storage_status_from_flash(
        drv_adapter_flash_config_delete(FLASH_DEV_EXTERNAL, key));
}

storage_status_t storage_log_append(storage_log_time_t timestamp,
                                    const void *data,
                                    size_t size)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    return storage_status_from_flash(drv_adapter_flash_log_append(
        FLASH_DEV_EXTERNAL, timestamp, data, size));
}

size_t storage_log_visit_latest(size_t max_entries,
                                storage_log_visitor_t visitor,
                                void *argument)
{
    if (!s_storage_ready) return 0U;
    return drv_adapter_flash_log_visit_latest(
        FLASH_DEV_EXTERNAL, max_entries, visitor, argument);
}

storage_status_t storage_log_clear(void)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    return storage_status_from_flash(
        drv_adapter_flash_log_clear(FLASH_DEV_EXTERNAL));
}

storage_status_t storage_image_get_info(storage_area_info_t *info)
{
    return storage_area_get_info(FLASH_REGION_IMAGE, info);
}

storage_status_t storage_image_read(uint32_t offset, void *data, size_t size)
{
    return storage_area_read(FLASH_REGION_IMAGE, offset, data, size);
}

storage_status_t storage_image_write(uint32_t offset, const void *data,
                                     size_t size)
{
    return storage_area_write(FLASH_REGION_IMAGE, offset, data, size);
}

storage_status_t storage_image_erase(uint32_t offset, size_t size)
{
    return storage_area_erase(FLASH_REGION_IMAGE, offset, size);
}

storage_status_t storage_ota_get_info(storage_area_info_t *info)
{
    return storage_area_get_info(FLASH_REGION_OTA, info);
}

storage_status_t storage_ota_read(uint32_t offset, void *data, size_t size)
{
    return storage_area_read(FLASH_REGION_OTA, offset, data, size);
}

storage_status_t storage_ota_write(uint32_t offset, const void *data,
                                   size_t size)
{
    return storage_area_write(FLASH_REGION_OTA, offset, data, size);
}

storage_status_t storage_ota_erase(uint32_t offset, size_t size)
{
    return storage_area_erase(FLASH_REGION_OTA, offset, size);
}
