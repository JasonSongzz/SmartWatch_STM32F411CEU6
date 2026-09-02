#include "osal_internal_mutex.h"
#include "os_freertos.h"

#if (OSAL_BACKEND == OSAL_BACKEND_FREERTOS)

int32_t os_mutex_create_impl(osal_mutex_handle_t *p_mutex_handle)
{
    int32_t ret;
    xSemaphoreHandle cur_mutex_handle;
    cur_mutex_handle = xSemaphoreCreateMutex();
    if (cur_mutex_handle == NULL)
    {
        ret = OSAL_ERROR;
    }
    else
    {
        *p_mutex_handle = (osal_mutex_handle_t)cur_mutex_handle;
        ret = OSAL_SUCCESS;
    }
    return ret;
}

void os_mutex_delete_impl(osal_mutex_handle_t mutex_handle)
{
    vSemaphoreDelete((xSemaphoreHandle)mutex_handle);
}

int32_t os_mutex_give_impl(osal_mutex_handle_t mutex_handle)
{
    int32_t ret;
    BaseType_t status;
    xSemaphoreHandle handle = (xSemaphoreHandle)mutex_handle;

    OSAL_CHECK_POINTER(handle);

    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    status = xSemaphoreGive(handle);

    if (status == pdPASS)
    {
        ret = OSAL_SUCCESS;
    }
    else
    {
        ret = OSAL_ERROR;
    }
    return ret;
}

int32_t os_mutex_take_impl(osal_mutex_handle_t mutex_handle,
                           osal_time_ms_t timeout_ms)
{
    int32_t ret;
    BaseType_t status;
    xSemaphoreHandle handle = (xSemaphoreHandle)mutex_handle;
    OSAL_CHECK_POINTER(handle);

    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    status = xSemaphoreTake(handle,
                            os_freertos_ms_to_ticks(timeout_ms));

    if (pdPASS == status)
    {
        ret = OSAL_SUCCESS;
    }
    else
    {
        ret = OSAL_ERROR_TIMEOUT;
    }
    return ret;
}

#endif /* OSAL_BACKEND */
