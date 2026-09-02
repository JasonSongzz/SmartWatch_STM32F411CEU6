#include "osal_internal_timer.h"
#include "osal_internal_globaldefs.h"
#include "osal_internal_heap.h"

//#include "app_log.h"

int32_t osal_timer_create(osal_timer_handle_t *p_timer_handle,
                          const char *timer_name,
                          osal_time_ms_t period_ms,
                          uint8_t auto_reload,
                          osal_timer_cb_function_t timer_cb,
                          void *arg)
{
    int32_t ret;
    osal_timer_internal_record_t *p_timer_record;

    OSAL_CHECK_POINTER(p_timer_handle);
    OSAL_CHECK_POINTER(timer_cb);
    OSAL_CHECK_SIZE(period_ms);
    OSAL_CHECK_STRING(timer_name, OSAL_NAME_MAX_LENGTH, OSAL_ERR_NAME_TOO_LONG);

    p_timer_record = os_heap_malloc_impl(sizeof(osal_timer_internal_record_t));
    if (p_timer_record == NULL) {
        return OSAL_ERROR;
    }

    memset(p_timer_record, 0, sizeof(*p_timer_record));
    memcpy(p_timer_record->timer_name, timer_name, strlen(timer_name) + 1U);
    p_timer_record->period_ms = period_ms;
    p_timer_record->auto_reload = auto_reload;
    p_timer_record->timer_id.func = timer_cb;
    p_timer_record->timer_id.arg = arg;
    ret = os_timer_create_impl(p_timer_handle, p_timer_record);
    if (ret != OSAL_SUCCESS) {
        os_heap_free_impl(p_timer_record);
    }
    return ret;
}

int32_t osal_timer_start(osal_timer_handle_t timer_handle,
                         osal_time_ms_t command_timeout_ms)
{
    int32_t ret;
    OSAL_CHECK_POINTER(timer_handle);
    ret = os_timer_start_impl(timer_handle, command_timeout_ms);
    return ret;
}

int32_t osal_timer_stop(osal_timer_handle_t timer_handle,
                        osal_time_ms_t command_timeout_ms)
{
    OSAL_CHECK_POINTER(timer_handle);
    return os_timer_stop_impl(timer_handle, command_timeout_ms);
}

int32_t osal_timer_period_change(osal_timer_handle_t timer_handle,
                                 osal_time_ms_t new_period_ms,
                                 osal_time_ms_t command_timeout_ms)
{
    OSAL_CHECK_POINTER(timer_handle);
    OSAL_CHECK_SIZE(new_period_ms);
    return os_timer_period_change_impl(timer_handle, new_period_ms,
                                       command_timeout_ms);
}

int32_t osal_timer_delete(osal_timer_handle_t timer_handle,
                          osal_time_ms_t command_timeout_ms)
{
    OSAL_CHECK_POINTER(timer_handle);
    return os_timer_delete_impl(timer_handle, command_timeout_ms);
}

int32_t osal_timer_reset(osal_timer_handle_t timer_handle,
                         osal_time_ms_t command_timeout_ms)
{
    OSAL_CHECK_POINTER(timer_handle);
    return os_timer_reset_impl(timer_handle, command_timeout_ms);
}

osal_time_ms_t osal_timer_period_get(osal_timer_handle_t timer_handle)
{
    if (timer_handle == NULL) {
        return 0U;
    }
    return os_timer_period_get_impl(timer_handle);
}
