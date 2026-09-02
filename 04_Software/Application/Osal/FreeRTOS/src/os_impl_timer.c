#include "osal_internal_timer.h"
#include "osal_internal_globaldefs.h"
#include "osal_internal_heap.h"
#include "os_freertos.h"
#include <stddef.h>
//#include "app_log.h"

#if (OSAL_BACKEND == OSAL_BACKEND_FREERTOS)

static void os_timer_cb(TimerHandle_t xTimer)
{
    osal_timer_t *timer = pvTimerGetTimerID(xTimer);
    if (timer != NULL && timer->func != NULL) {
        timer->func(timer->timer_handle, timer->arg);
    }
}

static void os_timer_free_record(void *arg, uint32_t ignored)
{
    (void)ignored;
    os_heap_free_impl(arg);
}

int32_t os_timer_create_impl(osal_timer_handle_t *p_timer_handle, osal_timer_internal_record_t *timer_record)
{
    int32_t ret = OSAL_SUCCESS;
    TickType_t period_ticks = os_freertos_ms_to_ticks(timer_record->period_ms);
    TimerHandle_t cur_timer_handle;

    if (period_ticks == 0U) {
        period_ticks = 1U;
    }
    cur_timer_handle = xTimerCreate(timer_record->timer_name, period_ticks,
                                    (timer_record->auto_reload != 0U) ? pdTRUE : pdFALSE,
                                    &timer_record->timer_id, os_timer_cb);
    *p_timer_handle = (osal_timer_handle_t)cur_timer_handle;
    timer_record->timer_id.timer_handle = *p_timer_handle;
    if (NULL == *p_timer_handle)
    {
        ret = OSAL_INVALID_POINTER;
    }
    return ret;
}

int32_t os_timer_start_impl(osal_timer_handle_t timer_handle,
                            osal_time_ms_t command_timeout_ms)
{
    int32_t ret = OSAL_SUCCESS;
    BaseType_t status;

    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    status = xTimerStart((TimerHandle_t)timer_handle,
                         os_freertos_ms_to_ticks(command_timeout_ms));
    if (pdFAIL == status)
    {
        ret = OSAL_ERROR;
    }
    return ret;
}

int32_t os_timer_stop_impl(osal_timer_handle_t timer_handle,
                           osal_time_ms_t command_timeout_ms)
{
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    return (xTimerStop((TimerHandle_t)timer_handle,
                       os_freertos_ms_to_ticks(command_timeout_ms)) == pdPASS)
               ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_timer_period_change_impl(osal_timer_handle_t timer_handle,
                                    osal_time_ms_t new_period_ms,
                                    osal_time_ms_t command_timeout_ms)
{
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    return (xTimerChangePeriod((TimerHandle_t)timer_handle,
                               os_freertos_ms_to_ticks(new_period_ms),
                               os_freertos_ms_to_ticks(command_timeout_ms)) == pdPASS)
               ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_timer_delete_impl(osal_timer_handle_t timer_handle,
                             osal_time_ms_t command_timeout_ms)
{
    TimerHandle_t handle = (TimerHandle_t)timer_handle;
    osal_timer_t *timer_id = (osal_timer_t *)pvTimerGetTimerID(handle);
    osal_timer_internal_record_t *record = NULL;

    if (timer_id != NULL) {
        record = (osal_timer_internal_record_t *)((uint8_t *)timer_id -
                 offsetof(osal_timer_internal_record_t, timer_id));
    }
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    if (xTimerDelete(handle,
                     os_freertos_ms_to_ticks(command_timeout_ms)) == pdFAIL) {
        return OSAL_ERROR;
    }
    if (record != NULL &&
        xTimerPendFunctionCall(os_timer_free_record, record, 0U,
                              os_freertos_ms_to_ticks(command_timeout_ms)) == pdFAIL) {
        return OSAL_ERROR;
    }
    return OSAL_SUCCESS;
}

int32_t os_timer_reset_impl(osal_timer_handle_t timer_handle,
                            osal_time_ms_t command_timeout_ms)
{
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    return (xTimerReset((TimerHandle_t)timer_handle,
                        os_freertos_ms_to_ticks(command_timeout_ms)) == pdPASS)
               ? OSAL_SUCCESS : OSAL_ERROR;
}

osal_time_ms_t os_timer_period_get_impl(osal_timer_handle_t timer_handle)
{
    return os_freertos_ticks_to_ms(
        xTimerGetPeriod((TimerHandle_t)timer_handle));
}

#endif /* OSAL_BACKEND */
