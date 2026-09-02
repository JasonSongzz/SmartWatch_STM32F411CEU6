#ifndef __OSAL_INTERNAL_TASK_H__
#define __OSAL_INTERNAL_TASK_H__

#include "osal_task.h"
#include "osal_internal_globaldefs.h"

typedef struct
{
    char task_name[OSAL_NAME_MAX_LENGTH];
    size_t stack_size;
    osal_priority_t priority;
    osal_task_entry entry_function_pointer;
    osal_task_entry delete_hook_pointer;
    void *entry_arg;
    osal_stackptr_t stack_pointer;
    osal_task_handle_t *task_handle;
} osal_task_internal_record_t;

int32_t os_task_create_impl(osal_task_internal_record_t *p_task);

void os_task_delete_impl(osal_task_handle_t task_handle);

void os_task_start_impl(void);

void os_task_suspend_impl(osal_task_handle_t task_handle);

int32_t os_task_resume_impl(osal_task_handle_t task_handle);

int32_t os_task_resume_from_isr_impl(osal_task_handle_t task_handle);

void os_scheduler_lock_impl(void);

void os_scheduler_unlock_impl(void);

void os_task_delay_impl(uint32_t ticks);

void os_task_delay_ms_impl(uint32_t ms);

void os_task_delay_until_impl(uint32_t *p_last_wake_time, uint32_t ticks);

void os_task_delay_until_ms_impl(uint32_t *p_last_wake_time, uint32_t ms);

void os_enter_critical_impl(void);

void os_exit_critical_impl(void);

osal_irq_state_t os_enter_critical_from_isr_impl(void);

void os_exit_critical_from_isr_impl(osal_irq_state_t state);

void os_task_disable_interrupts_impl(void);

void os_task_enable_interrupts_impl(void);

int32_t os_port_yield_impl(void);

osal_tick_type_t os_task_get_tick_count_impl(void);

osal_time_ms_t os_time_get_ms_impl(void);

size_t os_task_get_stack_high_water_mark_impl(osal_task_handle_t task_handle);

osal_task_handle_t os_task_get_current_handle_impl(void);

int32_t os_task_notify_give_impl(osal_task_handle_t task_handle);

int32_t os_task_notify_give_from_isr_impl(osal_task_handle_t task_handle);

uint32_t os_task_notify_take_impl(uint32_t clear_on_exit,
                                  osal_time_ms_t timeout_ms);

int32_t os_idle_hook_register_impl(osal_idle_hook_t hook, void *argument);

#endif // __OSAL_INTERNAL_TASK_H__
