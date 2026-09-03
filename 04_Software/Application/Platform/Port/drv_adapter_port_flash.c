#include "drv_adapter_port_flash.h"

#include "bsp_spiflash_driver.h"
#include "drv_adapter_flash.h"
#include "fal.h"
#include "flashdb.h"
#include "osal.h"
#include "spi.h"
#include "spiflash_layout.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define FLASH_CONFIG_RECORD_MAGIC   0x43464731UL /* CFG1 */
#define FLASH_CONFIG_RECORD_VERSION 1U

#define FLASH_PORT_SPI_HANDLE             (&hspi2)
#define FLASH_PORT_CS_PORT                GPIOB
#define FLASH_PORT_CS_PIN                 GPIO_PIN_12
#define FLASH_PORT_CS_CLOCK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
#define FLASH_PORT_TRANSFER_TIMEOUT_MS    1000U

typedef struct {
    uint32_t magic;
    uint32_t format_version;
    uint32_t sequence;
    uint32_t payload_size;
    uint32_t crc32;
} flash_config_header_t;

typedef union {
    uint32_t alignment;
    uint8_t bytes[sizeof(flash_config_header_t) +
                  FLASH_CONFIG_MAX_DATA_SIZE];
} flash_config_buffer_t;

typedef struct {
    osal_mutex_handle_t mutex;
} flash_db_context_t;

typedef struct {
    flash_log_visitor_t visitor;
    void *argument;
    size_t limit;
    size_t count;
} flash_log_iter_context_t;

typedef struct {
    SPI_HandleTypeDef *spi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    osal_mutex_handle_t mutex;
    bool mutex_ready;
} flash_bus_context_t;

static bool s_registered;
static uint32_t s_registered_index;
static bool s_fal_flash_ready;
static bool s_flash_ready;
static bsp_spiflash_driver_t s_spiflash_driver;
static flash_bus_context_t s_bus;

static struct fdb_kvdb s_config_main_db;
static struct fdb_kvdb s_config_backup_db;
static struct fdb_tsdb s_log_db;
static flash_db_context_t s_config_main_context;
static flash_db_context_t s_config_backup_context;
static flash_db_context_t s_log_context;
static osal_mutex_handle_t s_config_api_mutex;
static osal_mutex_handle_t s_log_api_mutex;
static flash_config_buffer_t s_config_buffer_main;
static flash_config_buffer_t s_config_buffer_backup;
static uint8_t s_log_read_buffer[FLASH_LOG_MAX_DATA_SIZE];

static int fal_port_flash_init(void);
static int fal_port_flash_read(long offset, uint8_t *buffer, size_t size);
static int fal_port_flash_write(long offset, const uint8_t *buffer, size_t size);
static int fal_port_flash_erase(long offset, size_t size);
static bool flash_device_prepare(void);
static spiflash_status_t flash_bus_init(void *context);
static spiflash_status_t flash_bus_write_read(void *context,
                                              const uint8_t *write_buffer,
                                              size_t write_size,
                                              uint8_t *read_buffer,
                                              size_t read_size);
static spiflash_status_t flash_bus_lock(void *context, uint32_t timeout_ms);
static spiflash_status_t flash_bus_unlock(void *context);

static const spiflash_spi_driver_interface_t s_spi_interface = {
    .bus_context = &s_bus,
    .pf_spi_init = flash_bus_init,
    .pf_write_read = flash_bus_write_read,
    .pf_lock = flash_bus_lock,
    .pf_unlock = flash_bus_unlock,
};

static const spiflash_yield_interface_t s_yield_interface = {
    .pf_rtos_yield = osal_task_delay_ms,
};

struct fal_flash_dev g_fal_spi_flash = {
    .name = FAL_SPI_FLASH_DEV_NAME,
    .addr = 0U,
    .len = SPIFLASH_TOTAL_SIZE,
    .blk_size = SPIFLASH_SECTOR_SIZE,
    .ops = {
        .init = fal_port_flash_init,
        .read = fal_port_flash_read,
        .write = fal_port_flash_write,
        .erase = fal_port_flash_erase,
    },
    .write_gran = 1U,
};

static fdb_time_t flash_log_get_time(void)
{
    return s_log_db.last_time < INT64_MAX ? s_log_db.last_time + 1
                                          : s_log_db.last_time;
}

static bool flash_mutex_ensure(osal_mutex_handle_t *mutex)
{
    return *mutex != NULL || osal_mutex_create(mutex) == OSAL_SUCCESS;
}

static void flash_db_lock(fdb_db_t db)
{
    flash_db_context_t *context = (flash_db_context_t *)db->user_data;

    if (context != NULL && context->mutex != NULL)
        (void)osal_mutex_take(context->mutex, OSAL_WAIT_FOREVER);
}

static void flash_db_unlock(fdb_db_t db)
{
    flash_db_context_t *context = (flash_db_context_t *)db->user_data;

    if (context != NULL && context->mutex != NULL)
        (void)osal_mutex_give(context->mutex);
}

static bool flash_key_valid(const char *key)
{
    size_t length;

    if (key == NULL) return false;
    length = strlen(key);
    return length > 0U && length < FDB_KV_NAME_MAX;
}

static uint32_t flash_config_crc(const flash_config_header_t *header,
                                 const uint8_t *payload)
{
    uint32_t crc = fdb_calc_crc32(0U, header,
                                  offsetof(flash_config_header_t, crc32));
    return fdb_calc_crc32(crc, payload, header->payload_size);
}

static bool flash_config_read_record(fdb_kvdb_t db,
                                     const char *key,
                                     flash_config_buffer_t *buffer,
                                     size_t *record_size)
{
    struct fdb_blob blob;
    flash_config_header_t *header;
    size_t read_size;
    size_t expected_size;

    read_size = fdb_kv_get_blob(db, key,
                                fdb_blob_make(&blob, buffer->bytes,
                                              sizeof(buffer->bytes)));
    if (read_size < sizeof(flash_config_header_t) ||
        blob.saved.len != read_size) {
        return false;
    }

    header = (flash_config_header_t *)buffer->bytes;
    if (header->magic != FLASH_CONFIG_RECORD_MAGIC ||
        header->format_version != FLASH_CONFIG_RECORD_VERSION ||
        header->payload_size == 0U ||
        header->payload_size > FLASH_CONFIG_MAX_DATA_SIZE) {
        return false;
    }

    expected_size = sizeof(flash_config_header_t) + header->payload_size;
    if (read_size != expected_size ||
        header->crc32 != flash_config_crc(
                             header, buffer->bytes + sizeof(*header))) {
        return false;
    }

    if (record_size != NULL) *record_size = expected_size;
    return true;
}

static bool flash_sequence_newer(uint32_t left, uint32_t right)
{
    return (int32_t)(left - right) > 0;
}

static fdb_err_t flash_config_write_record(
    fdb_kvdb_t db, const char *key, const flash_config_buffer_t *buffer,
    size_t record_size)
{
    struct fdb_blob blob;

    return fdb_kv_set_blob(db, key,
                           fdb_blob_make(&blob, buffer->bytes, record_size));
}

static bool flash_log_iter_callback(fdb_tsl_t tsl, void *argument)
{
    flash_log_iter_context_t *context =
        (flash_log_iter_context_t *)argument;
    struct fdb_blob blob;
    size_t size;

    if (tsl->status != FDB_TSL_WRITE) return false;

    fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, s_log_read_buffer,
                                       sizeof(s_log_read_buffer)));
    if (blob.saved.len > sizeof(s_log_read_buffer)) return false;

    size = fdb_blob_read((fdb_db_t)&s_log_db, &blob);
    if (size != blob.saved.len) return false;

    context->count++;
    if (context->visitor(tsl->time, s_log_read_buffer, size,
                         context->argument)) {
        return true;
    }
    return context->count >= context->limit;
}

static bool flash_port_init(flash_drv_t *dev)
{
    bool rollover = true;

    (void)dev;
    if (s_flash_ready) return true;
    if (!flash_device_prepare() ||
        !flash_mutex_ensure(&s_config_main_context.mutex) ||
        !flash_mutex_ensure(&s_config_backup_context.mutex) ||
        !flash_mutex_ensure(&s_log_context.mutex) ||
        !flash_mutex_ensure(&s_config_api_mutex) ||
        !flash_mutex_ensure(&s_log_api_mutex)) {
        return false;
    }

    fdb_kvdb_control(&s_config_main_db, FDB_KVDB_CTRL_SET_LOCK,
                     flash_db_lock);
    fdb_kvdb_control(&s_config_main_db, FDB_KVDB_CTRL_SET_UNLOCK,
                     flash_db_unlock);
    if (fdb_kvdb_init(&s_config_main_db, "config_main_db",
                      SPIFLASH_CONFIG_MAIN_PARTITION_NAME, NULL,
                      &s_config_main_context) != FDB_NO_ERR) {
        return false;
    }

    fdb_kvdb_control(&s_config_backup_db, FDB_KVDB_CTRL_SET_LOCK,
                     flash_db_lock);
    fdb_kvdb_control(&s_config_backup_db, FDB_KVDB_CTRL_SET_UNLOCK,
                     flash_db_unlock);
    if (fdb_kvdb_init(&s_config_backup_db, "config_backup_db",
                      SPIFLASH_CONFIG_BACKUP_PARTITION_NAME, NULL,
                      &s_config_backup_context) != FDB_NO_ERR) {
        return false;
    }

    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_SET_LOCK, flash_db_lock);
    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_SET_UNLOCK, flash_db_unlock);
    if (fdb_tsdb_init(&s_log_db, "system_log_db",
                      SPIFLASH_LOG_PARTITION_NAME, flash_log_get_time,
                      FLASH_LOG_MAX_DATA_SIZE,
                      &s_log_context) != FDB_NO_ERR) {
        return false;
    }
    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_SET_ROLLOVER, &rollover);

    s_flash_ready = true;
    return true;
}

static bool flash_port_get_info(flash_drv_t *dev, flash_dev_info_t *info)
{
    spiflash_info_t bsp_info;

    (void)dev;
    if (info == NULL ||
        spiflash_get_info(&s_spiflash_driver, &bsp_info) != SPIFLASH_OK) {
        return false;
    }

    info->capacity = bsp_info.capacity;
    info->erase_size = bsp_info.erase_size;
    info->write_granularity_bits = bsp_info.write_granularity_bits;
    return true;
}

static bool flash_port_get_region_info(flash_drv_t *dev,
                                       flash_region_t region,
                                       flash_region_info_t *info)
{
    (void)dev;
    if (info == NULL) return false;

    info->erase_size = SPIFLASH_SECTOR_SIZE;
    switch (region) {
    case FLASH_REGION_OTA:
        info->name = SPIFLASH_OTA_PARTITION_NAME;
        info->offset = SPIFLASH_OTA_OFFSET;
        info->size = SPIFLASH_OTA_SIZE;
        return true;
    case FLASH_REGION_CONFIG_MAIN:
        info->name = SPIFLASH_CONFIG_MAIN_PARTITION_NAME;
        info->offset = SPIFLASH_CONFIG_MAIN_OFFSET;
        info->size = SPIFLASH_CONFIG_MAIN_SIZE;
        return true;
    case FLASH_REGION_CONFIG_BACKUP:
        info->name = SPIFLASH_CONFIG_BACKUP_PARTITION_NAME;
        info->offset = SPIFLASH_CONFIG_BACKUP_OFFSET;
        info->size = SPIFLASH_CONFIG_BACKUP_SIZE;
        return true;
    case FLASH_REGION_LOG:
        info->name = SPIFLASH_LOG_PARTITION_NAME;
        info->offset = SPIFLASH_LOG_OFFSET;
        info->size = SPIFLASH_LOG_SIZE;
        return true;
    case FLASH_REGION_IMAGE:
        info->name = SPIFLASH_IMAGE_PARTITION_NAME;
        info->offset = SPIFLASH_IMAGE_OFFSET;
        info->size = SPIFLASH_IMAGE_SIZE;
        return true;
    default:
        return false;
    }
}

static bool flash_port_read(flash_drv_t *dev, uint32_t address,
                            void *buffer, size_t size)
{
    (void)dev;
    return spiflash_read(&s_spiflash_driver, address, buffer, size) ==
           SPIFLASH_OK;
}

static bool flash_port_write(flash_drv_t *dev, uint32_t address,
                             const void *buffer, size_t size)
{
    (void)dev;
    return spiflash_write(&s_spiflash_driver, address, buffer, size) ==
           SPIFLASH_OK;
}

static bool flash_port_erase(flash_drv_t *dev, uint32_t address, size_t size)
{
    (void)dev;
    return spiflash_erase(&s_spiflash_driver, address, size) == SPIFLASH_OK;
}

static flash_status_t flash_port_config_save(flash_drv_t *dev,
                                             const char *key,
                                             const void *data,
                                             size_t size)
{
    flash_config_header_t *header;
    bool main_valid;
    bool backup_valid;
    bool main_ok;
    bool backup_ok;
    uint32_t latest_sequence = 0U;
    size_t record_size;

    (void)dev;
    if (!s_flash_ready) return FLASH_STATUS_NOT_READY;
    if (!flash_key_valid(key) || data == NULL || size == 0U ||
        size > FLASH_CONFIG_MAX_DATA_SIZE) {
        return FLASH_STATUS_ERROR_PARAM;
    }
    if (osal_mutex_take(s_config_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return FLASH_STATUS_IO_ERROR;

    main_valid = flash_config_read_record(&s_config_main_db, key,
                                          &s_config_buffer_main, NULL);
    backup_valid = flash_config_read_record(&s_config_backup_db, key,
                                            &s_config_buffer_backup, NULL);
    if (main_valid) {
        latest_sequence =
            ((flash_config_header_t *)s_config_buffer_main.bytes)->sequence;
    }
    if (backup_valid) {
        uint32_t backup_sequence =
            ((flash_config_header_t *)s_config_buffer_backup.bytes)->sequence;
        if (!main_valid ||
            flash_sequence_newer(backup_sequence, latest_sequence)) {
            latest_sequence = backup_sequence;
        }
    }

    header = (flash_config_header_t *)s_config_buffer_main.bytes;
    header->magic = FLASH_CONFIG_RECORD_MAGIC;
    header->format_version = FLASH_CONFIG_RECORD_VERSION;
    header->sequence = latest_sequence + 1U;
    header->payload_size = (uint32_t)size;
    memcpy(s_config_buffer_main.bytes + sizeof(*header), data, size);
    header->crc32 = flash_config_crc(
        header, s_config_buffer_main.bytes + sizeof(*header));
    record_size = sizeof(*header) + size;

    backup_ok = flash_config_write_record(&s_config_backup_db, key,
                                          &s_config_buffer_main,
                                          record_size) == FDB_NO_ERR;
    main_ok = flash_config_write_record(&s_config_main_db, key,
                                        &s_config_buffer_main,
                                        record_size) == FDB_NO_ERR;
    (void)osal_mutex_give(s_config_api_mutex);

    if (main_ok && backup_ok) return FLASH_STATUS_OK;
    if (main_ok || backup_ok) return FLASH_STATUS_DEGRADED;
    return FLASH_STATUS_IO_ERROR;
}

static flash_status_t flash_port_config_load(flash_drv_t *dev,
                                             const char *key,
                                             void *data,
                                             size_t *size)
{
    flash_config_buffer_t *selected;
    flash_config_buffer_t *other;
    flash_config_header_t *selected_header;
    flash_config_header_t *other_header;
    fdb_kvdb_t other_db;
    bool main_valid;
    bool backup_valid;
    bool repair_needed;
    size_t main_size = 0U;
    size_t backup_size = 0U;
    size_t selected_size;
    flash_status_t status = FLASH_STATUS_OK;

    (void)dev;
    if (!s_flash_ready) return FLASH_STATUS_NOT_READY;
    if (!flash_key_valid(key) || size == NULL)
        return FLASH_STATUS_ERROR_PARAM;
    if (osal_mutex_take(s_config_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return FLASH_STATUS_IO_ERROR;

    main_valid = flash_config_read_record(&s_config_main_db, key,
                                          &s_config_buffer_main, &main_size);
    backup_valid = flash_config_read_record(&s_config_backup_db, key,
                                            &s_config_buffer_backup,
                                            &backup_size);
    if (!main_valid && !backup_valid) {
        (void)osal_mutex_give(s_config_api_mutex);
        return FLASH_STATUS_NOT_FOUND;
    }

    if (main_valid &&
        (!backup_valid ||
         !flash_sequence_newer(
             ((flash_config_header_t *)s_config_buffer_backup.bytes)->sequence,
             ((flash_config_header_t *)s_config_buffer_main.bytes)->sequence))) {
        selected = &s_config_buffer_main;
        selected_size = main_size;
        other = &s_config_buffer_backup;
        other_db = &s_config_backup_db;
        repair_needed = !backup_valid;
    } else {
        selected = &s_config_buffer_backup;
        selected_size = backup_size;
        other = &s_config_buffer_main;
        other_db = &s_config_main_db;
        repair_needed = !main_valid;
    }

    selected_header = (flash_config_header_t *)selected->bytes;
    if (!repair_needed) {
        other_header = (flash_config_header_t *)other->bytes;
        repair_needed =
            selected_size !=
                (selected == &s_config_buffer_main ? backup_size : main_size) ||
            selected_header->sequence != other_header->sequence ||
            memcmp(selected->bytes, other->bytes, selected_size) != 0;
    }

    if (data == NULL || *size < selected_header->payload_size) {
        *size = selected_header->payload_size;
        (void)osal_mutex_give(s_config_api_mutex);
        return FLASH_STATUS_NO_SPACE;
    }
    memcpy(data, selected->bytes + sizeof(*selected_header),
           selected_header->payload_size);
    *size = selected_header->payload_size;

    if (repair_needed &&
        flash_config_write_record(other_db, key, selected,
                                  selected_size) != FDB_NO_ERR) {
        status = FLASH_STATUS_DEGRADED;
    }
    (void)osal_mutex_give(s_config_api_mutex);
    return status;
}

static flash_status_t flash_port_config_delete(flash_drv_t *dev,
                                               const char *key)
{
    bool main_ok;
    bool backup_ok;

    (void)dev;
    if (!s_flash_ready) return FLASH_STATUS_NOT_READY;
    if (!flash_key_valid(key)) return FLASH_STATUS_ERROR_PARAM;
    if (osal_mutex_take(s_config_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return FLASH_STATUS_IO_ERROR;

    backup_ok = fdb_kv_del(&s_config_backup_db, key) == FDB_NO_ERR;
    main_ok = fdb_kv_del(&s_config_main_db, key) == FDB_NO_ERR;
    (void)osal_mutex_give(s_config_api_mutex);

    if (main_ok && backup_ok) return FLASH_STATUS_OK;
    if (main_ok || backup_ok) return FLASH_STATUS_DEGRADED;
    return FLASH_STATUS_IO_ERROR;
}

static flash_status_t flash_port_log_append(flash_drv_t *dev,
                                            flash_log_time_t timestamp,
                                            const void *data,
                                            size_t size)
{
    struct fdb_blob blob;
    fdb_time_t last_time;
    fdb_err_t result;

    (void)dev;
    if (!s_flash_ready) return FLASH_STATUS_NOT_READY;
    if (data == NULL || size == 0U || size > FLASH_LOG_MAX_DATA_SIZE)
        return FLASH_STATUS_ERROR_PARAM;
    if (osal_mutex_take(s_log_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return FLASH_STATUS_IO_ERROR;

    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_GET_LAST_TIME, &last_time);
    if (timestamp <= last_time) {
        if (last_time == INT64_MAX) {
            (void)osal_mutex_give(s_log_api_mutex);
            return FLASH_STATUS_NO_SPACE;
        }
        timestamp = last_time + 1;
    }
    result = fdb_tsl_append_with_ts(&s_log_db,
                                    fdb_blob_make(&blob, data, size),
                                    timestamp);
    (void)osal_mutex_give(s_log_api_mutex);
    return result == FDB_NO_ERR ? FLASH_STATUS_OK : FLASH_STATUS_IO_ERROR;
}

static size_t flash_port_log_visit_latest(flash_drv_t *dev,
                                          size_t max_entries,
                                          flash_log_visitor_t visitor,
                                          void *argument)
{
    flash_log_iter_context_t context;

    (void)dev;
    if (!s_flash_ready || visitor == NULL || max_entries == 0U) return 0U;

    context.visitor = visitor;
    context.argument = argument;
    context.limit = max_entries;
    context.count = 0U;
    fdb_tsl_iter_reverse(&s_log_db, flash_log_iter_callback, &context);
    return context.count;
}

static flash_status_t flash_port_log_clear(flash_drv_t *dev)
{
    (void)dev;
    if (!s_flash_ready) return FLASH_STATUS_NOT_READY;
    if (osal_mutex_take(s_log_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return FLASH_STATUS_IO_ERROR;
    fdb_tsl_clean(&s_log_db);
    (void)osal_mutex_give(s_log_api_mutex);
    return FLASH_STATUS_OK;
}

bool drv_adapter_port_flash_register(uint32_t index)
{
    if (s_registered) return index == s_registered_index;

    const flash_drv_t driver = {
        .idx = index,
        .user_data = NULL,
        .init = flash_port_init,
        .get_info = flash_port_get_info,
        .get_region_info = flash_port_get_region_info,
        .read = flash_port_read,
        .write = flash_port_write,
        .erase = flash_port_erase,
        .config_save = flash_port_config_save,
        .config_load = flash_port_config_load,
        .config_delete = flash_port_config_delete,
        .log_append = flash_port_log_append,
        .log_visit_latest = flash_port_log_visit_latest,
        .log_clear = flash_port_log_clear,
    };

    if (!drv_adapter_flash_reg(index, &driver)) return false;

    s_registered_index = index;
    s_registered = true;
    return true;
}

static bool fal_port_range_valid(long offset, size_t size)
{
    if (offset < 0 || (size_t)offset > g_fal_spi_flash.len) return false;
    return size <= g_fal_spi_flash.len - (size_t)offset;
}

static bool flash_device_prepare(void)
{
    spiflash_info_t info;

    if (s_fal_flash_ready) return true;
    if (spiflash_inst(&s_spiflash_driver, &s_spi_interface,
                      &s_yield_interface) != SPIFLASH_OK ||
        spiflash_get_info(&s_spiflash_driver, &info) != SPIFLASH_OK ||
        info.capacity != SPIFLASH_TOTAL_SIZE ||
        info.erase_size != SPIFLASH_SECTOR_SIZE) {
        return false;
    }

    g_fal_spi_flash.blk_size = info.erase_size;
    g_fal_spi_flash.write_gran = info.write_granularity_bits;
    s_fal_flash_ready = true;
    return true;
}

static int fal_port_flash_init(void)
{
    return flash_device_prepare() ? 0 : -1;
}

static int fal_port_flash_read(long offset, uint8_t *buffer, size_t size)
{
    if (!s_fal_flash_ready || !fal_port_range_valid(offset, size) ||
        (size > 0U && buffer == NULL)) {
        return -1;
    }
    if (size == 0U) return 0;

    return spiflash_read(&s_spiflash_driver, (uint32_t)offset,
                         buffer, size) == SPIFLASH_OK
               ? (int)size : -1;
}

static int fal_port_flash_write(long offset, const uint8_t *buffer, size_t size)
{
    if (!s_fal_flash_ready || !fal_port_range_valid(offset, size) ||
        (size > 0U && buffer == NULL)) {
        return -1;
    }
    if (size == 0U) return 0;

    return spiflash_write(&s_spiflash_driver, (uint32_t)offset,
                          buffer, size) == SPIFLASH_OK
               ? (int)size : -1;
}

static int fal_port_flash_erase(long offset, size_t size)
{
    if (!s_fal_flash_ready || !fal_port_range_valid(offset, size) ||
        ((uint32_t)offset % g_fal_spi_flash.blk_size) != 0U ||
        (size % g_fal_spi_flash.blk_size) != 0U) {
        return -1;
    }
    if (size == 0U) return 0;

    return spiflash_erase(&s_spiflash_driver, (uint32_t)offset,
                          size) == SPIFLASH_OK
               ? (int)size : -1;
}

static void flash_bus_cs_write(const flash_bus_context_t *context,
                               GPIO_PinState state)
{
    HAL_GPIO_WritePin(context->cs_port, context->cs_pin, state);
}

static HAL_StatusTypeDef flash_bus_transmit(SPI_HandleTypeDef *spi,
                                            const uint8_t *buffer,
                                            size_t size)
{
    while (size > 0U) {
        uint16_t chunk = size > UINT16_MAX ? UINT16_MAX : (uint16_t)size;
        HAL_StatusTypeDef status = HAL_SPI_Transmit(
            spi, (uint8_t *)buffer, chunk, FLASH_PORT_TRANSFER_TIMEOUT_MS);

        if (status != HAL_OK) return status;
        buffer += chunk;
        size -= chunk;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef flash_bus_receive(SPI_HandleTypeDef *spi,
                                           uint8_t *buffer,
                                           size_t size)
{
    while (size > 0U) {
        uint16_t chunk = size > UINT16_MAX ? UINT16_MAX : (uint16_t)size;
        HAL_StatusTypeDef status = HAL_SPI_Receive(
            spi, buffer, chunk, FLASH_PORT_TRANSFER_TIMEOUT_MS);

        if (status != HAL_OK) return status;
        buffer += chunk;
        size -= chunk;
    }
    return HAL_OK;
}

static spiflash_status_t flash_bus_init(void *context)
{
    flash_bus_context_t *bus = (flash_bus_context_t *)context;
    GPIO_InitTypeDef gpio = {0};

    if (bus == NULL) return SPIFLASH_ERROR_PARAMETER;
    if (!bus->mutex_ready) {
        if (osal_mutex_create(&bus->mutex) != OSAL_SUCCESS)
            return SPIFLASH_ERROR_IO;
        bus->mutex_ready = true;
    }

    bus->spi = FLASH_PORT_SPI_HANDLE;
    bus->cs_port = FLASH_PORT_CS_PORT;
    bus->cs_pin = FLASH_PORT_CS_PIN;
    FLASH_PORT_CS_CLOCK_ENABLE();
    gpio.Pin = bus->cs_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(bus->cs_port, &gpio);
    flash_bus_cs_write(bus, GPIO_PIN_SET);
    return SPIFLASH_OK;
}

static spiflash_status_t flash_bus_write_read(void *context,
                                              const uint8_t *write_buffer,
                                              size_t write_size,
                                              uint8_t *read_buffer,
                                              size_t read_size)
{
    flash_bus_context_t *bus = (flash_bus_context_t *)context;
    HAL_StatusTypeDef hal_status;

    if (bus == NULL || bus->spi == NULL || bus->cs_port == NULL)
        return SPIFLASH_ERROR_NOT_READY;
    if (write_size > 0U && write_buffer == NULL)
        return SPIFLASH_ERROR_PARAMETER;
    if (read_size > 0U && read_buffer == NULL)
        return SPIFLASH_ERROR_PARAMETER;

    flash_bus_cs_write(bus, GPIO_PIN_RESET);
    hal_status = flash_bus_transmit(bus->spi, write_buffer, write_size);
    if (hal_status == HAL_OK)
        hal_status = flash_bus_receive(bus->spi, read_buffer, read_size);
    flash_bus_cs_write(bus, GPIO_PIN_SET);

    if (hal_status == HAL_OK) return SPIFLASH_OK;
    return hal_status == HAL_TIMEOUT ? SPIFLASH_ERROR_TIMEOUT
                                     : SPIFLASH_ERROR_IO;
}

static spiflash_status_t flash_bus_lock(void *context, uint32_t timeout_ms)
{
    flash_bus_context_t *bus = (flash_bus_context_t *)context;

    if (bus == NULL || !bus->mutex_ready) return SPIFLASH_ERROR_NOT_READY;
    return osal_mutex_take(bus->mutex, timeout_ms) == OSAL_SUCCESS
               ? SPIFLASH_OK : SPIFLASH_ERROR_TIMEOUT;
}

static spiflash_status_t flash_bus_unlock(void *context)
{
    flash_bus_context_t *bus = (flash_bus_context_t *)context;

    if (bus == NULL || !bus->mutex_ready) return SPIFLASH_ERROR_NOT_READY;
    return osal_mutex_give(bus->mutex) == OSAL_SUCCESS
               ? SPIFLASH_OK : SPIFLASH_ERROR_IO;
}
