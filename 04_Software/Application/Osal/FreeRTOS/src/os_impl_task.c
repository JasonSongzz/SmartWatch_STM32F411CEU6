#include "osal_internal_task.h"
#include "os_freertos.h"
//#include "app_log.h"

#if (OSAL_BACKEND == OSAL_BACKEND_FREERTOS)

#define OSAL_CHECK_APINAME(str) \
    OSAL_CHECK_STRING(str, OSAL_NAME_MAX_LENGTH, OSAL_ERR_NAME_TOO_LONG)

static osal_idle_hook_t s_idle_hook;
static void *s_idle_hook_argument;

static UBaseType_t osal_priority_to_native(osal_priority_t priority)
{
    static const UBaseType_t priority_map[OSAL_PRIORITY_COUNT] = {
        [OSAL_PRIORITY_DEFAULT] = 12U,
        [OSAL_PRIORITY_BACKGROUND] = 4U,
        [OSAL_PRIORITY_LOW] = 8U,
        [OSAL_PRIORITY_NORMAL] = 12U,
        [OSAL_PRIORITY_ABOVE_NORMAL] = 14U,
        [OSAL_PRIORITY_HIGH] = 15U,
        [OSAL_PRIORITY_REALTIME] = 16U,
        [OSAL_PRIORITY_CRITICAL] = 17U,
    };

    configASSERT(priority < OSAL_PRIORITY_COUNT);
    configASSERT(priority_map[priority] < configMAX_PRIORITIES);
    return priority_map[priority];
}

/* FreeRTOS xTaskCreate 第三参数为栈深度（单位：StackType_t 字数），工程内调用方按「字节」传入 */
static configSTACK_DEPTH_TYPE osal_stack_bytes_to_words(size_t stack_bytes)
{
    size_t words = (stack_bytes + sizeof(StackType_t) - 1U) / sizeof(StackType_t);
    if (words < (size_t)configMINIMAL_STACK_SIZE) {
        words = (size_t)configMINIMAL_STACK_SIZE;
    }
    return (configSTACK_DEPTH_TYPE)words;
}

int32_t os_task_create_impl(osal_task_internal_record_t *p_task)
{
    int32_t ret = OSAL_SUCCESS;
    OSAL_CHECK_APINAME(p_task->task_name);

    BaseType_t error_code;
    error_code = xTaskCreate(p_task->entry_function_pointer, p_task->task_name,
                             osal_stack_bytes_to_words(p_task->stack_size),
                             p_task->entry_arg,
                             osal_priority_to_native(p_task->priority),
                             (TaskHandle_t *)p_task->task_handle);
    if (pdPASS != error_code)
    {
        ret = OSAL_ERROR;
    }
    return ret;
}

#if (INCLUDE_vTaskDelete == 1)
void os_task_delete_impl(osal_task_handle_t task_handle)
{
    vTaskDelete(task_handle);
}
#endif

void os_task_start_impl(void)
{
    vTaskStartScheduler();
}

#if (INCLUDE_vTaskSuspend == 1)
void os_task_suspend_impl(osal_task_handle_t task_handle)
{
    vTaskSuspend(task_handle);
}
#endif

void os_scheduler_lock_impl(void)
{
    vTaskSuspendAll();
}

void os_scheduler_unlock_impl(void)
{
    (void)xTaskResumeAll();
}

int32_t os_task_resume_impl(osal_task_handle_t task_handle)
{
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    if (task_handle == NULL) {
        return OSAL_INVALID_POINTER;
    }
    vTaskResume((TaskHandle_t)task_handle);
    return OSAL_SUCCESS;
}

int32_t os_task_resume_from_isr_impl(osal_task_handle_t task_handle)
{
    BaseType_t higher_priority_task_woken;

    if (!OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERROR;
    }
    if (task_handle == NULL) {
        return OSAL_INVALID_POINTER;
    }
    higher_priority_task_woken = xTaskResumeFromISR((TaskHandle_t)task_handle);
    portYIELD_FROM_ISR(higher_priority_task_woken);
    return OSAL_SUCCESS;
}

#if (INCLUDE_vTaskDelay == 1)
void os_task_delay_impl(uint32_t ticks)
{
    vTaskDelay((TickType_t)ticks);
}

void os_task_delay_ms_impl(uint32_t ms)
{
    vTaskDelay(os_freertos_ms_to_ticks(ms));
}
#endif

#if (INCLUDE_vTaskDelayUntil == 1)
void os_task_delay_until_impl(uint32_t *p_last_wake_time, uint32_t ticks)
{
    TickType_t last;

    if (p_last_wake_time == NULL)
    {
        return;
    }

    /* 用局部变量避免直接把uint32_t*强转成TickType_t*（更安全一些） */
    last = (TickType_t)(*p_last_wake_time);
    vTaskDelayUntil(&last, (TickType_t)ticks);
    *p_last_wake_time = (uint32_t)last;
}

void os_task_delay_until_ms_impl(uint32_t *p_last_wake_time, uint32_t ms)
{
    uint32_t ticks = (uint32_t)os_freertos_ms_to_ticks(ms);

    /* 避免 ms 很小导致 ticks=0 变成“忙跑” */
    if ((ticks == 0U) && (ms != 0U))
    {
        ticks = 1U;
    }

    os_task_delay_until_impl(p_last_wake_time, ticks);
}

#endif

void os_enter_critical_impl(void)
{
    configASSERT(!OS_FREERTOS_IS_IN_ISR());
    taskENTER_CRITICAL();
}

void os_exit_critical_impl(void)
{
    configASSERT(!OS_FREERTOS_IS_IN_ISR());
    taskEXIT_CRITICAL();
}

osal_irq_state_t os_enter_critical_from_isr_impl(void)
{
    configASSERT(OS_FREERTOS_IS_IN_ISR());
    return (osal_irq_state_t)taskENTER_CRITICAL_FROM_ISR();
}

void os_exit_critical_from_isr_impl(osal_irq_state_t state)
{
    configASSERT(OS_FREERTOS_IS_IN_ISR());
    taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)state);
}

int32_t os_port_yield_impl(void)
{
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    portYIELD();
    return OSAL_SUCCESS;
}

void os_task_disable_interrupts_impl(void)
{
    taskDISABLE_INTERRUPTS();
}

void os_task_enable_interrupts_impl(void)
{
    taskENABLE_INTERRUPTS();
}

enum {
    OSAL_FATAL_NONE = 0U,
    OSAL_FATAL_ASSERT = 1U,
    OSAL_FATAL_STACK_OVERFLOW = 2U,
    OSAL_FATAL_MALLOC_FAILED = 3U
};

volatile uint32_t g_osal_fatal_reason;
volatile TaskHandle_t g_osal_fatal_task;
volatile const char *g_osal_fatal_task_name;
volatile const char *g_osal_fatal_file;
volatile uint32_t g_osal_fatal_line;

static void osal_fatal_stop(uint32_t reason)
{
    g_osal_fatal_reason = reason;
    taskDISABLE_INTERRUPTS();
    for (;;) {
        /* The independent watchdog resets the target; globals remain inspectable meanwhile. */
    }
}

void osal_freertos_assert_failed(const char *file, uint32_t line)
{
    g_osal_fatal_file = file;
    g_osal_fatal_line = line;
    g_osal_fatal_task = xTaskGetCurrentTaskHandle();
    osal_fatal_stop(OSAL_FATAL_ASSERT);
}

#if (configCHECK_FOR_STACK_OVERFLOW > 0)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    g_osal_fatal_task = xTask;
    g_osal_fatal_task_name = pcTaskName;
    osal_fatal_stop(OSAL_FATAL_STACK_OVERFLOW);
}
#endif

#if (configUSE_MALLOC_FAILED_HOOK > 0)
void vApplicationMallocFailedHook()
{
    g_osal_fatal_task = xTaskGetCurrentTaskHandle();
    osal_fatal_stop(OSAL_FATAL_MALLOC_FAILED);
}
#endif

osal_tick_type_t os_task_get_tick_count_impl(void)
{
    osal_tick_type_t os_ticks = xTaskGetTickCount();
    return os_ticks;
}

osal_time_ms_t os_time_get_ms_impl(void)
{
    return os_freertos_ticks_to_ms(xTaskGetTickCount());
}

size_t os_task_get_stack_high_water_mark_impl(osal_task_handle_t task_handle)
{
    UBaseType_t words = uxTaskGetStackHighWaterMark((TaskHandle_t)task_handle);
    return (size_t)words * sizeof(StackType_t);
}

#if (configUSE_TASK_NOTIFICATIONS == 1)
osal_task_handle_t os_task_get_current_handle_impl(void)
{
    return (osal_task_handle_t)xTaskGetCurrentTaskHandle();
}

int32_t os_task_notify_give_impl(osal_task_handle_t task_handle)
{
    if (OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERR_IN_ISR;
    }
    if (task_handle == NULL) {
        return OSAL_INVALID_POINTER;
    }
    (void)xTaskNotifyGive((TaskHandle_t)task_handle);
    return OSAL_SUCCESS;
}

int32_t os_task_notify_give_from_isr_impl(osal_task_handle_t task_handle)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (!OS_FREERTOS_IS_IN_ISR()) {
        return OSAL_ERROR;
    }
    if (task_handle == NULL) {
        return OSAL_INVALID_POINTER;
    }
    vTaskNotifyGiveFromISR((TaskHandle_t)task_handle,
                           &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
    return OSAL_SUCCESS;
}

uint32_t os_task_notify_take_impl(uint32_t clear_on_exit,
                                  osal_time_ms_t timeout_ms)
{
    BaseType_t clear = (clear_on_exit != 0U) ? pdTRUE : pdFALSE;
    if (OS_FREERTOS_IS_IN_ISR()) {
        return 0U;
    }
    return ulTaskNotifyTake(clear, os_freertos_ms_to_ticks(timeout_ms));
}
#else
osal_task_handle_t os_task_get_current_handle_impl(void)
{
    return NULL;
}

int32_t os_task_notify_give_impl(osal_task_handle_t task_handle)
{
    (void)task_handle;
    return OSAL_ERR_OPERATION_NOT_SUPPORTED;
}

int32_t os_task_notify_give_from_isr_impl(osal_task_handle_t task_handle)
{
    (void)task_handle;
    return OSAL_ERR_OPERATION_NOT_SUPPORTED;
}

uint32_t os_task_notify_take_impl(uint32_t clear_on_exit,
                                  osal_time_ms_t timeout_ms)
{
    (void)clear_on_exit;
    (void)timeout_ms;
    return 0U;
}
#endif

int32_t os_idle_hook_register_impl(osal_idle_hook_t hook, void *argument)
{
#if (configUSE_IDLE_HOOK == 1)
    BaseType_t scheduler_state;

    if (hook == NULL) {
        return OSAL_INVALID_POINTER;
    }
    scheduler_state = xTaskGetSchedulerState();
    if (scheduler_state != taskSCHEDULER_NOT_STARTED) {
        taskENTER_CRITICAL();
    }
    s_idle_hook = hook;
    s_idle_hook_argument = argument;
    if (scheduler_state != taskSCHEDULER_NOT_STARTED) {
        taskEXIT_CRITICAL();
    }
    return OSAL_SUCCESS;
#else
    (void)hook;
    (void)argument;
    return OSAL_ERR_OPERATION_NOT_SUPPORTED;
#endif
}

#if (configUSE_IDLE_HOOK == 1)
void vApplicationIdleHook(void)
{
    if (s_idle_hook != NULL) {
        s_idle_hook(s_idle_hook_argument);
    }
}
#endif

#endif /* OSAL_BACKEND */
