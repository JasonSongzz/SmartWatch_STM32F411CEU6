#include "drv_adapter_flash.h"

#include <stddef.h>

static flash_drv_t s_driver[FLASH_DEV_MAX];

static flash_status_t flash_region_resolve(uint32_t index,
                                           flash_region_t region,
                                           uint32_t offset, size_t size,
                                           uint32_t *address,
                                           flash_region_info_t *resolved_info)
{
    flash_dev_info_t device_info;
    flash_region_info_t region_info;

    if (address == NULL || region >= FLASH_REGION_MAX)
        return FLASH_STATUS_ERROR_PARAM;
    if (index >= FLASH_DEV_MAX ||
        !drv_adapter_flash_get_info(index, &device_info) ||
        !drv_adapter_flash_region_get_info(index, region, &region_info) ||
        region_info.offset > device_info.capacity ||
        region_info.size > device_info.capacity - region_info.offset) {
        return FLASH_STATUS_NOT_READY;
    }
    if (offset > region_info.size || size > region_info.size - offset)
        return FLASH_STATUS_OUT_OF_RANGE;

    *address = region_info.offset + offset;
    if (resolved_info != NULL) *resolved_info = region_info;
    return FLASH_STATUS_OK;
}

bool drv_adapter_flash_reg(uint32_t index, const flash_drv_t *driver)
{
    if (index >= FLASH_DEV_MAX || driver == NULL ||
        driver->init == NULL || driver->get_info == NULL ||
        driver->get_region_info == NULL ||
        driver->read == NULL || driver->write == NULL ||
        driver->erase == NULL || driver->config_save == NULL ||
        driver->config_load == NULL || driver->config_delete == NULL ||
        driver->log_append == NULL || driver->log_visit_latest == NULL ||
        driver->log_clear == NULL) {
        return false;
    }

    s_driver[index] = *driver;
    s_driver[index].idx = index;
    return true;
}

bool drv_adapter_flash_init(uint32_t index)
{
    return index < FLASH_DEV_MAX && s_driver[index].init != NULL &&
           s_driver[index].init(&s_driver[index]);
}

bool drv_adapter_flash_get_info(uint32_t index, flash_dev_info_t *info)
{
    return index < FLASH_DEV_MAX && info != NULL &&
           s_driver[index].get_info != NULL &&
           s_driver[index].get_info(&s_driver[index], info);
}

bool drv_adapter_flash_region_get_info(uint32_t index, flash_region_t region,
                                       flash_region_info_t *info)
{
    return index < FLASH_DEV_MAX && region < FLASH_REGION_MAX &&
           info != NULL && s_driver[index].get_region_info != NULL &&
           s_driver[index].get_region_info(&s_driver[index], region, info);
}

bool drv_adapter_flash_read(uint32_t index, uint32_t address,
                            void *buffer, size_t size)
{
    return index < FLASH_DEV_MAX && (size == 0U || buffer != NULL) &&
           s_driver[index].read != NULL &&
           s_driver[index].read(&s_driver[index], address, buffer, size);
}

bool drv_adapter_flash_write(uint32_t index, uint32_t address,
                             const void *buffer, size_t size)
{
    return index < FLASH_DEV_MAX && (size == 0U || buffer != NULL) &&
           s_driver[index].write != NULL &&
           s_driver[index].write(&s_driver[index], address, buffer, size);
}

bool drv_adapter_flash_erase(uint32_t index, uint32_t address, size_t size)
{
    return index < FLASH_DEV_MAX && s_driver[index].erase != NULL &&
           s_driver[index].erase(&s_driver[index], address, size);
}

flash_status_t drv_adapter_flash_region_read(uint32_t index,
                                             flash_region_t region,
                                             uint32_t offset, void *buffer,
                                             size_t size)
{
    uint32_t address;
    flash_status_t status;

    if (size > 0U && buffer == NULL) return FLASH_STATUS_ERROR_PARAM;
    status = flash_region_resolve(index, region, offset, size, &address, NULL);
    if (status != FLASH_STATUS_OK) return status;
    return drv_adapter_flash_read(index, address, buffer, size)
               ? FLASH_STATUS_OK : FLASH_STATUS_IO_ERROR;
}

flash_status_t drv_adapter_flash_region_write(uint32_t index,
                                              flash_region_t region,
                                              uint32_t offset,
                                              const void *buffer,
                                              size_t size)
{
    uint32_t address;
    flash_status_t status;

    if (size > 0U && buffer == NULL) return FLASH_STATUS_ERROR_PARAM;
    status = flash_region_resolve(index, region, offset, size, &address, NULL);
    if (status != FLASH_STATUS_OK) return status;
    return drv_adapter_flash_write(index, address, buffer, size)
               ? FLASH_STATUS_OK : FLASH_STATUS_IO_ERROR;
}

flash_status_t drv_adapter_flash_region_erase(uint32_t index,
                                              flash_region_t region,
                                              uint32_t offset, size_t size)
{
    uint32_t address;
    flash_region_info_t info;
    flash_status_t status;

    status = flash_region_resolve(index, region, offset, size, &address, &info);
    if (status != FLASH_STATUS_OK) return status;
    if (size > 0U &&
        (info.erase_size == 0U || (address % info.erase_size) != 0U ||
         (size % info.erase_size) != 0U)) {
        return FLASH_STATUS_ALIGNMENT_ERROR;
    }
    return drv_adapter_flash_erase(index, address, size)
               ? FLASH_STATUS_OK : FLASH_STATUS_IO_ERROR;
}

flash_status_t drv_adapter_flash_config_save(uint32_t index, const char *key,
                                             const void *data, size_t size)
{
    if (index >= FLASH_DEV_MAX || s_driver[index].config_save == NULL)
        return FLASH_STATUS_NOT_READY;
    return s_driver[index].config_save(&s_driver[index], key, data, size);
}

flash_status_t drv_adapter_flash_config_load(uint32_t index, const char *key,
                                             void *data, size_t *size)
{
    if (index >= FLASH_DEV_MAX || s_driver[index].config_load == NULL)
        return FLASH_STATUS_NOT_READY;
    return s_driver[index].config_load(&s_driver[index], key, data, size);
}

flash_status_t drv_adapter_flash_config_delete(uint32_t index,
                                               const char *key)
{
    if (index >= FLASH_DEV_MAX || s_driver[index].config_delete == NULL)
        return FLASH_STATUS_NOT_READY;
    return s_driver[index].config_delete(&s_driver[index], key);
}

flash_status_t drv_adapter_flash_log_append(uint32_t index,
                                            flash_log_time_t timestamp,
                                            const void *data, size_t size)
{
    if (index >= FLASH_DEV_MAX || s_driver[index].log_append == NULL)
        return FLASH_STATUS_NOT_READY;
    return s_driver[index].log_append(&s_driver[index], timestamp, data, size);
}

size_t drv_adapter_flash_log_visit_latest(uint32_t index, size_t max_entries,
                                          flash_log_visitor_t visitor,
                                          void *argument)
{
    if (index >= FLASH_DEV_MAX || s_driver[index].log_visit_latest == NULL)
        return 0U;
    return s_driver[index].log_visit_latest(&s_driver[index], max_entries,
                                            visitor, argument);
}

flash_status_t drv_adapter_flash_log_clear(uint32_t index)
{
    if (index >= FLASH_DEV_MAX || s_driver[index].log_clear == NULL)
        return FLASH_STATUS_NOT_READY;
    return s_driver[index].log_clear(&s_driver[index]);
}
