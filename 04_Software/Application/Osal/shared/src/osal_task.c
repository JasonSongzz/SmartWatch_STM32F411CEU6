#include "osal_internal_task.h"
#include "osal_internal_globaldefs.h"
#include "osal_internal_heap.h"

//#include "app_log.h"


int32_t osal_task_create(const char *task_name, osal_task_entry func_pointer, size_t stack_size,
                         osal_priority_t priority, osal_task_handle_t *task_handle, void *argument)
{
    int32_t ret;
    osal_task_internal_record_t task;

    OSAL_CHECK_POINTER(func_pointer);
    OSAL_CHECK_POINTER(task_handle);
    OSAL_CHECK_SIZE(stack_size);
    OSAL_CHECK_STRING(task_name, OSAL_NAME_MAX_LENGTH, OSAL_ERR_NAME_TOO_LONG);
    if (priority >= OSAL_PRIORITY_COUNT) {
        return OSAL_ERR_INVALID_PRIORITY;
    }

    memset(&task, 0, sizeof(osal_task_internal_record_t));
    memcpy(task.task_name, task_name, strlen(task_name) + 1U);
    task.task_handle = task_handle;
    task.stack_size = stack_size;
    task.priority = priority;
    task.entry_function_pointer = func_pointer;
    task.entry_arg = argument;

    ret = os_task_create_impl(&task);
    return ret;
}

void osal_task_delete(osal_task_handle_t osal_task_handle)
{
    os_task_delete_impl(osal_task_handle);
}

void osal_task_start(void)
{
    os_task_start_impl();
}

void osal_task_suspend(osal_task_handle_t osal_task_handle)
{
    os_task_suspend_impl(osal_task_handle);
}

void osal_scheduler_lock(void)
{
    os_scheduler_lock_impl();
}

void osal_scheduler_unlock(void)
{
    os_scheduler_unlock_impl();
}

int32_t osal_task_resume(osal_task_handle_t osal_task_handle)
{
    return os_task_resume_impl(osal_task_handle);
}

int32_t osal_task_resume_from_isr(osal_task_handle_t osal_task_handle)
{
    return os_task_resume_from_isr_impl(osal_task_handle);
}

void osal_task_delay(int32_t ticks)
{
    os_task_delay_impl((uint32_t)ticks);
}

void osal_task_delay_ms(uint32_t ms)
{
    os_task_delay_ms_impl(ms);
}

void osal_task_delay_until(osal_tick_type_t *p_last_wake_time, int32_t ticks)
{
    if (p_last_wake_time == NULL)
    {
        return;
    }
    os_task_delay_until_impl(p_last_wake_time, (uint32_t)ticks);
}

void osal_task_delay_until_ms(osal_tick_type_t *p_last_wake_time, uint32_t ms)
{
    if (p_last_wake_time == NULL)
    {
        return;
    }
    os_task_delay_until_ms_impl(p_last_wake_time, ms);
}

void osal_enter_critical(void)
{
    os_enter_critical_impl();
}

void osal_exit_critical(void)
{
    os_exit_critical_impl();
}

osal_irq_state_t osal_enter_critical_from_isr(void)
{
    return os_enter_critical_from_isr_impl();
}

void osal_exit_critical_from_isr(osal_irq_state_t state)
{
    os_exit_critical_from_isr_impl(state);
}

int32_t osal_port_yield(void)
{
    return os_port_yield_impl();
}

void osal_task_enable_interrupts(void)
{
    os_task_enable_interrupts_impl();
}

void osal_task_disable_interrupts(void)
{
    os_task_disable_interrupts_impl();
}

osal_tick_type_t osal_task_get_tick_count(void)
{
    osal_tick_type_t osal_ticks = os_task_get_tick_count_impl();
    return osal_ticks;
}

osal_time_ms_t osal_time_get_ms(void)
{
    return os_time_get_ms_impl();
}

size_t osal_task_get_stack_high_water_mark(osal_task_handle_t task_handle)
{
    return os_task_get_stack_high_water_mark_impl(task_handle);
}

osal_task_handle_t osal_task_get_current_handle(void)
{
    return os_task_get_current_handle_impl();
}

int32_t osal_task_notify_give(osal_task_handle_t task_handle)
{
    return os_task_notify_give_impl(task_handle);
}

int32_t osal_task_notify_give_from_isr(osal_task_handle_t task_handle)
{
    return os_task_notify_give_from_isr_impl(task_handle);
}

uint32_t osal_task_notify_take(uint32_t clear_on_exit,
                               osal_time_ms_t timeout_ms)
{
    return os_task_notify_take_impl(clear_on_exit, timeout_ms);
}

int32_t osal_idle_hook_register(osal_idle_hook_t hook, void *argument)
{
    if (hook == NULL) {
        return OSAL_INVALID_POINTER;
    }
    return os_idle_hook_register_impl(hook, argument);
}
