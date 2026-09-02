#include "osal_internal_queue.h"
#include "os_freertos.h"
//#include "app_log.h"

#if (OSAL_BACKEND == OSAL_BACKEND_FREERTOS)

int32_t os_queue_create_impl( size_t queue_depth, size_t data_size,osal_queue_handle_t *p_queue_handle)
{
    int32_t ret;
    xQueueHandle cur_queue_handle;
    cur_queue_handle = xQueueCreate(queue_depth, data_size);
    if (cur_queue_handle == NULL)
    {
        ret = OSAL_ERROR;
    }
    else
    {
        *p_queue_handle = (osal_queue_handle_t)cur_queue_handle;
        int32_t num = uxQueueMessagesWaiting(cur_queue_handle);
        (void)num;
        ret = OSAL_SUCCESS;
    }
    return ret;
}

void os_queue_delete_impl(osal_queue_handle_t queue_handle)
{
    vQueueDelete((xQueueHandle)queue_handle);
}

int32_t os_queue_send_impl(osal_queue_handle_t queue_handle, const void *data,
                           osal_time_ms_t timeout_ms)
{
    BaseType_t status;
    xQueueHandle handle = (xQueueHandle)queue_handle;

    OSAL_CHECK_POINTER(queue_handle);
    OSAL_CHECK_POINTER(data);
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    status = xQueueSend(handle, data,
                        os_freertos_ms_to_ticks(timeout_ms));
    if (status == pdPASS) {
        return OSAL_SUCCESS;
    }
    return (timeout_ms == OSAL_WAIT_NONE) ? OSAL_QUEUE_FULL
                                           : OSAL_QUEUE_TIMEOUT;
}

int32_t os_queue_receive_impl(osal_queue_handle_t queue_handle, void *data,
                              osal_time_ms_t timeout_ms)
{
    BaseType_t status;
    xQueueHandle handle = (xQueueHandle)queue_handle;

    OSAL_CHECK_POINTER(handle);
    OSAL_CHECK_POINTER(data);
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    status = xQueueReceive(handle, data,
                           os_freertos_ms_to_ticks(timeout_ms));
    if (status == pdPASS) {
        return OSAL_SUCCESS;
    }
    return (timeout_ms == OSAL_WAIT_NONE) ? OSAL_QUEUE_EMPTY
                                           : OSAL_QUEUE_TIMEOUT;
}

int32_t os_queue_send_from_isr_impl(osal_queue_handle_t queue_handle,
                                    const void *data)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t status;

    OSAL_CHECK_POINTER(queue_handle);
    OSAL_CHECK_POINTER(data);
    if (!OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERROR;
    }
    status = xQueueSendFromISR((QueueHandle_t)queue_handle, data,
                               &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
    return (status == pdPASS) ? OSAL_SUCCESS : OSAL_QUEUE_FULL;
}

int32_t os_queue_receive_from_isr_impl(osal_queue_handle_t queue_handle,
                                       void *data)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t status;

    OSAL_CHECK_POINTER(queue_handle);
    OSAL_CHECK_POINTER(data);
    if (!OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERROR;
    }
    status = xQueueReceiveFromISR((QueueHandle_t)queue_handle, data,
                                  &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
    return (status == pdPASS) ? OSAL_SUCCESS : OSAL_QUEUE_EMPTY;
}

int32_t os_queue_msg_waiting_impl(osal_queue_handle_t queue_handle)
{
    xQueueHandle handle = (xQueueHandle)queue_handle;
    if (handle == NULL || OS_FREERTOS_IS_IN_ISR()) {
        return 0;
    }
    return (int32_t)uxQueueMessagesWaiting(handle);
}

size_t os_queue_spaces_available_impl(osal_queue_handle_t queue_handle)
{
    if (queue_handle == NULL || OS_FREERTOS_IS_IN_ISR()) {
        return 0U;
    }
    return (size_t)uxQueueSpacesAvailable((QueueHandle_t)queue_handle);
}

#endif /* OSAL_BACKEND */
