#ifndef __OSAL_TASK_H__
#define __OSAL_TASK_H__

#include "common_types.h"
#include "osal_config.h"

typedef enum {
    OSAL_PRIORITY_DEFAULT = 0,
    OSAL_PRIORITY_BACKGROUND,
    OSAL_PRIORITY_LOW,
    OSAL_PRIORITY_NORMAL,
    OSAL_PRIORITY_ABOVE_NORMAL,
    OSAL_PRIORITY_HIGH,
    OSAL_PRIORITY_REALTIME,
    OSAL_PRIORITY_CRITICAL,
    OSAL_PRIORITY_COUNT
} osal_priority_t;

typedef void *osal_stackptr_t;

typedef void (*osal_task_entry)(void *);
typedef void (*osal_idle_hook_t)(void *argument);

/** Create a task. Stack size is expressed in bytes. */
int32_t osal_task_create(const char *task_name, osal_task_entry func_pointer,
                         size_t stack_size, osal_priority_t priority,
                         osal_task_handle_t *task_handle, void *argument);

/** Delete a task. NULL selects the current task when supported by the backend. */
void osal_task_delete(osal_task_handle_t task_handle);

void osal_task_start(void);

void osal_task_suspend(osal_task_handle_t task_handle);

int32_t osal_task_resume(osal_task_handle_t task_handle);

int32_t osal_task_resume_from_isr(osal_task_handle_t task_handle);

void osal_scheduler_lock(void);

void osal_scheduler_unlock(void);

void osal_task_delay(int32_t ticks);

void osal_task_delay_ms(uint32_t ms);

void osal_task_delay_until(osal_tick_type_t *last_wake_time, int32_t ticks);

/** Periodic delay with a persistent backend time state. */
void osal_task_delay_until_ms(osal_tick_type_t *last_wake_time,
                              uint32_t period_ms);

void osal_enter_critical(void);
void osal_exit_critical(void);

/** ISR-only critical section. Pass the returned state back unchanged. */
osal_irq_state_t osal_enter_critical_from_isr(void);

void osal_exit_critical_from_isr(osal_irq_state_t state);

int32_t osal_port_yield(void);

void osal_task_enable_interrupts(void);

void osal_task_disable_interrupts(void);

/** Backend scheduler time used only to initialize periodic-delay state. */
osal_tick_type_t osal_task_get_tick_count(void);

/** Monotonic time in milliseconds, with unsigned wrap-around semantics. */
osal_time_ms_t osal_time_get_ms(void);

/** Remaining unused stack in bytes. NULL selects the current task. */
size_t osal_task_get_stack_high_water_mark(osal_task_handle_t task_handle);

osal_task_handle_t osal_task_get_current_handle(void);

/** Give one lightweight notification to a target task. */
int32_t osal_task_notify_give(osal_task_handle_t task_handle);

int32_t osal_task_notify_give_from_isr(osal_task_handle_t task_handle);

/** Wait for and consume a task notification; zero means timeout. */
uint32_t osal_task_notify_take(uint32_t clear_on_exit,
                               osal_time_ms_t timeout_ms);

/** Register the single portable idle-thread hook used by system services. */
int32_t osal_idle_hook_register(osal_idle_hook_t hook, void *argument);

#endif /* __OSAL_TASK_H__ */
