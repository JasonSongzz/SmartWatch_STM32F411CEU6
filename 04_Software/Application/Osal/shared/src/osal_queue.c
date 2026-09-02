#include "osal_internal_queue.h"
#include "osal_internal_globaldefs.h"
// #include "osal_internal_idmap.h"

//#include "app_log.h"

int32_t osal_queue_create(size_t queue_depth, size_t data_size,osal_queue_handle_t *p_queue_handle)
{
    int32_t ret;

    OSAL_CHECK_POINTER(p_queue_handle);
    OSAL_CHECK_SIZE(queue_depth);
    OSAL_CHECK_SIZE(data_size);
    ret = os_queue_create_impl(queue_depth, data_size,p_queue_handle);
    return ret;
}

void osal_queue_delete(osal_queue_handle_t queue_handle)
{
    if (queue_handle != NULL) {
        os_queue_delete_impl(queue_handle);
    }
}

int32_t osal_queue_send(osal_queue_handle_t queue_handle, const void *data,
                        osal_time_ms_t timeout_ms)
{
    return os_queue_send_impl(queue_handle, data, timeout_ms);
}

int32_t osal_queue_receive(osal_queue_handle_t queue_handle, void *data,
                           osal_time_ms_t timeout_ms)
{
    return os_queue_receive_impl(queue_handle, data, timeout_ms);
}

int32_t osal_queue_send_from_isr(osal_queue_handle_t queue_handle,
                                 const void *data)
{
    return os_queue_send_from_isr_impl(queue_handle, data);
}

int32_t osal_queue_receive_from_isr(osal_queue_handle_t queue_handle,
                                    void *data)
{
    return os_queue_receive_from_isr_impl(queue_handle, data);
}

int32_t osal_queue_msg_waiting(osal_queue_handle_t queue_handle)
{
    return os_queue_msg_waiting_impl(queue_handle);
}

size_t osal_queue_spaces_available(osal_queue_handle_t queue_handle)
{
    return os_queue_spaces_available_impl(queue_handle);
}
