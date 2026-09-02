#include "osal_internal_sema.h"
#include "os_freertos.h"
//#include "app_log.h"

#if (OSAL_BACKEND == OSAL_BACKEND_FREERTOS)

int32_t os_sema_countings_create_impl(osal_sema_handle_t *p_sema_handle, uint32_t max_count, uint32_t init_count)
{
    int32_t ret;
    xSemaphoreHandle cur_sema_handle;
    cur_sema_handle = xSemaphoreCreateCounting(max_count, init_count);
    if (cur_sema_handle == NULL)
    {
        ret = OSAL_ERROR;
    }
    else
    {
        *p_sema_handle = cur_sema_handle;
        ret = OSAL_SUCCESS;
    }
    return ret;
}

int32_t os_sema_binary_create_impl(osal_sema_handle_t *p_sema_handle)
{
    xSemaphoreHandle cur_sema_handle = xSemaphoreCreateBinary();
    if (cur_sema_handle == NULL)
    {
        return OSAL_INVALID_POINTER;
    }
    *p_sema_handle = cur_sema_handle;
    return OSAL_SUCCESS;
}

void os_sema_delete_impl(osal_sema_handle_t sema_handle)
{
    vSemaphoreDelete((xSemaphoreHandle)sema_handle);
}

int32_t os_sema_give_impl(osal_sema_handle_t sema_handle)
{
    int32_t ret;
    BaseType_t status;
    xSemaphoreHandle handle = (xSemaphoreHandle)sema_handle;

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

int32_t os_sema_give_from_isr_impl(osal_sema_handle_t sema_handle)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t status;

    OSAL_CHECK_POINTER(sema_handle);
    if (!OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERROR;
    }
    status = xSemaphoreGiveFromISR((SemaphoreHandle_t)sema_handle,
                                   &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
    return (status == pdPASS) ? OSAL_SUCCESS : OSAL_SEM_FAILURE;
}

int32_t os_sema_take_impl(osal_sema_handle_t sema_handle,
                          osal_time_ms_t timeout_ms)
{
    int32_t ret;
    BaseType_t status;
    xSemaphoreHandle handle = (xSemaphoreHandle)sema_handle;
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
        ret = OSAL_SEM_TIMEOUT;
    }
    return ret;
}

#endif /* OSAL_BACKEND */
