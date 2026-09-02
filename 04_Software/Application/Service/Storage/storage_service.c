#include "storage_service.h"

#include "flashdb.h"
#include "osal.h"
#include "sfud_w25q64.h"
#include "w25q64_layout.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define STORAGE_CONFIG_RECORD_MAGIC   0x43464731UL /* CFG1 */
#define STORAGE_CONFIG_RECORD_VERSION 1U
#define STORAGE_CONFIG_MAIN_PART      "config_main"
#define STORAGE_CONFIG_BACKUP_PART    "config_backup"
#define STORAGE_LOG_PART              "log_tsdb"

typedef struct {
    uint32_t magic;
    uint32_t format_version;
    uint32_t sequence;
    uint32_t payload_size;
    uint32_t crc32;
} storage_config_header_t;

typedef union {
    uint32_t alignment;
    uint8_t bytes[sizeof(storage_config_header_t) + STORAGE_CONFIG_MAX_DATA_SIZE];
} storage_config_buffer_t;

typedef struct {
    osal_mutex_handle_t mutex;
} storage_db_context_t;

typedef struct {
    storage_log_visitor_t visitor;
    void *argument;
    size_t limit;
    size_t count;
} storage_log_iter_context_t;

static struct fdb_kvdb s_config_main_db;
static struct fdb_kvdb s_config_backup_db;
static struct fdb_tsdb s_log_db;

static storage_db_context_t s_config_main_context;
static storage_db_context_t s_config_backup_context;
static storage_db_context_t s_log_context;
static osal_mutex_handle_t s_config_api_mutex;
static osal_mutex_handle_t s_log_api_mutex;

static storage_config_buffer_t s_config_buffer_main;
static storage_config_buffer_t s_config_buffer_backup;
static uint8_t s_log_read_buffer[STORAGE_LOG_MAX_DATA_SIZE];
static bool s_storage_ready;

static fdb_time_t storage_log_get_time(void)
{
    return s_log_db.last_time < INT64_MAX ? s_log_db.last_time + 1
                                          : s_log_db.last_time;
}

static bool storage_mutex_ensure(osal_mutex_handle_t *mutex)
{
    return *mutex != NULL || osal_mutex_create(mutex) == OSAL_SUCCESS;
}

static void storage_db_lock(fdb_db_t db)
{
    storage_db_context_t *context = (storage_db_context_t *)db->user_data;
    if (context != NULL && context->mutex != NULL) {
        (void)osal_mutex_take(context->mutex, OSAL_WAIT_FOREVER);
    }
}

static void storage_db_unlock(fdb_db_t db)
{
    storage_db_context_t *context = (storage_db_context_t *)db->user_data;
    if (context != NULL && context->mutex != NULL) {
        (void)osal_mutex_give(context->mutex);
    }
}

static bool storage_key_valid(const char *key)
{
    size_t length;
    if (key == NULL) return false;
    length = strlen(key);
    return length > 0U && length < FDB_KV_NAME_MAX;
}

static uint32_t storage_config_crc(const storage_config_header_t *header,
                                   const uint8_t *payload)
{
    uint32_t crc = fdb_calc_crc32(0U, header,
                                  offsetof(storage_config_header_t, crc32));
    return fdb_calc_crc32(crc, payload, header->payload_size);
}

static bool storage_config_read_record(fdb_kvdb_t db,
                                       const char *key,
                                       storage_config_buffer_t *buffer,
                                       size_t *record_size)
{
    struct fdb_blob blob;
    storage_config_header_t *header;
    size_t read_size;
    size_t expected_size;

    read_size = fdb_kv_get_blob(db, key,
                                fdb_blob_make(&blob, buffer->bytes,
                                              sizeof(buffer->bytes)));
    if (read_size < sizeof(storage_config_header_t) ||
        blob.saved.len != read_size) {
        return false;
    }

    header = (storage_config_header_t *)buffer->bytes;
    if (header->magic != STORAGE_CONFIG_RECORD_MAGIC ||
        header->format_version != STORAGE_CONFIG_RECORD_VERSION ||
        header->payload_size == 0U ||
        header->payload_size > STORAGE_CONFIG_MAX_DATA_SIZE) {
        return false;
    }

    expected_size = sizeof(storage_config_header_t) + header->payload_size;
    if (read_size != expected_size ||
        header->crc32 != storage_config_crc(header,
                                             buffer->bytes + sizeof(*header))) {
        return false;
    }

    if (record_size != NULL) *record_size = expected_size;
    return true;
}

static bool storage_sequence_newer(uint32_t left, uint32_t right)
{
    return (int32_t)(left - right) > 0;
}

static fdb_err_t storage_config_write_record(fdb_kvdb_t db,
                                             const char *key,
                                             const storage_config_buffer_t *buffer,
                                             size_t record_size)
{
    struct fdb_blob blob;
    return fdb_kv_set_blob(db, key,
                           fdb_blob_make(&blob, buffer->bytes, record_size));
}

static bool storage_log_iter_callback(fdb_tsl_t tsl, void *argument)
{
    storage_log_iter_context_t *context = (storage_log_iter_context_t *)argument;
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

storage_status_t storage_service_init(void)
{
    uint32_t sector_size = W25Q64_SECTOR_SIZE;
    bool rollover = true;

    if (s_storage_ready) return STORAGE_STATUS_OK;
    if (!sfud_w25q64_init()) return STORAGE_STATUS_IO_ERROR;

    if (!storage_mutex_ensure(&s_config_main_context.mutex) ||
        !storage_mutex_ensure(&s_config_backup_context.mutex) ||
        !storage_mutex_ensure(&s_log_context.mutex) ||
        !storage_mutex_ensure(&s_config_api_mutex) ||
        !storage_mutex_ensure(&s_log_api_mutex)) {
        return STORAGE_STATUS_IO_ERROR;
    }

    fdb_kvdb_control(&s_config_main_db, FDB_KVDB_CTRL_SET_SEC_SIZE, &sector_size);
    fdb_kvdb_control(&s_config_main_db, FDB_KVDB_CTRL_SET_LOCK, storage_db_lock);
    fdb_kvdb_control(&s_config_main_db, FDB_KVDB_CTRL_SET_UNLOCK, storage_db_unlock);
    if (fdb_kvdb_init(&s_config_main_db, "config_main_db",
                      STORAGE_CONFIG_MAIN_PART, NULL,
                      &s_config_main_context) != FDB_NO_ERR) {
        return STORAGE_STATUS_IO_ERROR;
    }

    fdb_kvdb_control(&s_config_backup_db, FDB_KVDB_CTRL_SET_SEC_SIZE, &sector_size);
    fdb_kvdb_control(&s_config_backup_db, FDB_KVDB_CTRL_SET_LOCK, storage_db_lock);
    fdb_kvdb_control(&s_config_backup_db, FDB_KVDB_CTRL_SET_UNLOCK, storage_db_unlock);
    if (fdb_kvdb_init(&s_config_backup_db, "config_backup_db",
                      STORAGE_CONFIG_BACKUP_PART, NULL,
                      &s_config_backup_context) != FDB_NO_ERR) {
        return STORAGE_STATUS_IO_ERROR;
    }

    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_SET_SEC_SIZE, &sector_size);
    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_SET_LOCK, storage_db_lock);
    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_SET_UNLOCK, storage_db_unlock);
    if (fdb_tsdb_init(&s_log_db, "system_log_db", STORAGE_LOG_PART,
                      storage_log_get_time, STORAGE_LOG_MAX_DATA_SIZE,
                      &s_log_context) != FDB_NO_ERR) {
        return STORAGE_STATUS_IO_ERROR;
    }
    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_SET_ROLLOVER, &rollover);

    s_storage_ready = true;
    return STORAGE_STATUS_OK;
}

bool storage_service_is_ready(void)
{
    return s_storage_ready;
}

storage_status_t storage_config_save(const char *key, const void *data, size_t size)
{
    storage_config_header_t *header;
    bool main_valid;
    bool backup_valid;
    bool main_ok;
    bool backup_ok;
    uint32_t latest_sequence = 0U;
    size_t record_size;
    struct fdb_blob blob;

    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    if (!storage_key_valid(key) || data == NULL || size == 0U ||
        size > STORAGE_CONFIG_MAX_DATA_SIZE) {
        return STORAGE_STATUS_ERROR_PARAM;
    }
    if (osal_mutex_take(s_config_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return STORAGE_STATUS_IO_ERROR;

    main_valid = storage_config_read_record(&s_config_main_db, key,
                                            &s_config_buffer_main, NULL);
    backup_valid = storage_config_read_record(&s_config_backup_db, key,
                                              &s_config_buffer_backup, NULL);
    if (main_valid) {
        latest_sequence = ((storage_config_header_t *)s_config_buffer_main.bytes)->sequence;
    }
    if (backup_valid) {
        uint32_t backup_sequence =
            ((storage_config_header_t *)s_config_buffer_backup.bytes)->sequence;
        if (!main_valid || storage_sequence_newer(backup_sequence, latest_sequence))
            latest_sequence = backup_sequence;
    }

    header = (storage_config_header_t *)s_config_buffer_main.bytes;
    header->magic = STORAGE_CONFIG_RECORD_MAGIC;
    header->format_version = STORAGE_CONFIG_RECORD_VERSION;
    header->sequence = latest_sequence + 1U;
    header->payload_size = (uint32_t)size;
    memcpy(s_config_buffer_main.bytes + sizeof(*header), data, size);
    header->crc32 = storage_config_crc(header,
                                       s_config_buffer_main.bytes + sizeof(*header));
    record_size = sizeof(*header) + size;

    backup_ok = fdb_kv_set_blob(&s_config_backup_db, key,
                                fdb_blob_make(&blob, s_config_buffer_main.bytes,
                                              record_size)) == FDB_NO_ERR;
    main_ok = storage_config_write_record(&s_config_main_db, key,
                                          &s_config_buffer_main,
                                          record_size) == FDB_NO_ERR;
    (void)osal_mutex_give(s_config_api_mutex);

    if (main_ok && backup_ok) return STORAGE_STATUS_OK;
    if (main_ok || backup_ok) return STORAGE_STATUS_DEGRADED;
    return STORAGE_STATUS_IO_ERROR;
}

storage_status_t storage_config_load(const char *key, void *data, size_t *size)
{
    storage_config_buffer_t *selected;
    storage_config_buffer_t *other;
    storage_config_header_t *selected_header;
    storage_config_header_t *other_header;
    fdb_kvdb_t other_db;
    bool main_valid;
    bool backup_valid;
    bool repair_needed;
    size_t main_size = 0U;
    size_t backup_size = 0U;
    size_t selected_size;
    storage_status_t status = STORAGE_STATUS_OK;

    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    if (!storage_key_valid(key) || size == NULL) return STORAGE_STATUS_ERROR_PARAM;
    if (osal_mutex_take(s_config_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return STORAGE_STATUS_IO_ERROR;

    main_valid = storage_config_read_record(&s_config_main_db, key,
                                            &s_config_buffer_main, &main_size);
    backup_valid = storage_config_read_record(&s_config_backup_db, key,
                                              &s_config_buffer_backup, &backup_size);
    if (!main_valid && !backup_valid) {
        (void)osal_mutex_give(s_config_api_mutex);
        return STORAGE_STATUS_NOT_FOUND;
    }

    if (main_valid && (!backup_valid ||
        !storage_sequence_newer(
            ((storage_config_header_t *)s_config_buffer_backup.bytes)->sequence,
            ((storage_config_header_t *)s_config_buffer_main.bytes)->sequence))) {
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

    selected_header = (storage_config_header_t *)selected->bytes;
    if (!repair_needed) {
        other_header = (storage_config_header_t *)other->bytes;
        repair_needed = selected_size != (selected == &s_config_buffer_main
                                           ? backup_size : main_size) ||
                        selected_header->sequence != other_header->sequence ||
                        memcmp(selected->bytes, other->bytes, selected_size) != 0;
    }

    if (data == NULL || *size < selected_header->payload_size) {
        *size = selected_header->payload_size;
        (void)osal_mutex_give(s_config_api_mutex);
        return STORAGE_STATUS_NO_SPACE;
    }
    memcpy(data, selected->bytes + sizeof(*selected_header),
           selected_header->payload_size);
    *size = selected_header->payload_size;

    if (repair_needed &&
        storage_config_write_record(other_db, key, selected,
                                    selected_size) != FDB_NO_ERR) {
        status = STORAGE_STATUS_DEGRADED;
    }
    (void)osal_mutex_give(s_config_api_mutex);
    return status;
}

storage_status_t storage_config_delete(const char *key)
{
    bool main_ok;
    bool backup_ok;
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    if (!storage_key_valid(key)) return STORAGE_STATUS_ERROR_PARAM;
    if (osal_mutex_take(s_config_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return STORAGE_STATUS_IO_ERROR;

    backup_ok = fdb_kv_del(&s_config_backup_db, key) == FDB_NO_ERR;
    main_ok = fdb_kv_del(&s_config_main_db, key) == FDB_NO_ERR;
    (void)osal_mutex_give(s_config_api_mutex);
    if (main_ok && backup_ok) return STORAGE_STATUS_OK;
    if (main_ok || backup_ok) return STORAGE_STATUS_DEGRADED;
    return STORAGE_STATUS_IO_ERROR;
}

storage_status_t storage_log_append(storage_log_time_t timestamp,
                                    const void *data,
                                    size_t size)
{
    struct fdb_blob blob;
    fdb_time_t last_time;
    fdb_err_t result;

    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    if (data == NULL || size == 0U || size > STORAGE_LOG_MAX_DATA_SIZE)
        return STORAGE_STATUS_ERROR_PARAM;
    if (osal_mutex_take(s_log_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return STORAGE_STATUS_IO_ERROR;

    fdb_tsdb_control(&s_log_db, FDB_TSDB_CTRL_GET_LAST_TIME, &last_time);
    if (timestamp <= last_time) {
        if (last_time == INT64_MAX) {
            (void)osal_mutex_give(s_log_api_mutex);
            return STORAGE_STATUS_NO_SPACE;
        }
        timestamp = last_time + 1;
    }
    result = fdb_tsl_append_with_ts(&s_log_db,
                                    fdb_blob_make(&blob, data, size),
                                    timestamp);
    (void)osal_mutex_give(s_log_api_mutex);
    return result == FDB_NO_ERR ? STORAGE_STATUS_OK : STORAGE_STATUS_IO_ERROR;
}

size_t storage_log_visit_latest(size_t max_entries,
                                storage_log_visitor_t visitor,
                                void *argument)
{
    storage_log_iter_context_t context;
    if (!s_storage_ready || visitor == NULL || max_entries == 0U) return 0U;

    context.visitor = visitor;
    context.argument = argument;
    context.limit = max_entries;
    context.count = 0U;
    fdb_tsl_iter_reverse(&s_log_db, storage_log_iter_callback, &context);
    return context.count;
}

storage_status_t storage_log_clear(void)
{
    if (!s_storage_ready) return STORAGE_STATUS_NOT_READY;
    if (osal_mutex_take(s_log_api_mutex, OSAL_WAIT_FOREVER) != OSAL_SUCCESS)
        return STORAGE_STATUS_IO_ERROR;
    fdb_tsl_clean(&s_log_db);
    (void)osal_mutex_give(s_log_api_mutex);
    return STORAGE_STATUS_OK;
}
