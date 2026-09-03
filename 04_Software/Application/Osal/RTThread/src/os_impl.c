#include "osal_internal_heap.h"
#include "osal_internal_mutex.h"
#include "osal_internal_queue.h"
#include "osal_internal_sema.h"
#include "osal_internal_task.h"
#include "osal_internal_timer.h"
#include "os_rtthread.h"

#if (OSAL_BACKEND == OSAL_BACKEND_RTTHREAD)

typedef struct os_rtthread_task
{
    rt_thread_t thread;
    rt_sem_t notification;
    osal_task_entry entry;
    void *argument;
    struct os_rtthread_task *next;
} os_rtthread_task_t;

typedef struct
{
    rt_mq_t queue;
    size_t item_size;
    size_t depth;
} os_rtthread_queue_t;

typedef struct
{
    rt_timer_t timer;
    osal_timer_internal_record_t *record;
} os_rtthread_timer_t;

static os_rtthread_task_t *s_tasks;
static osal_idle_hook_t s_idle_hook;
static void *s_idle_hook_argument;
static rt_base_t s_critical_state;
static uint32_t s_critical_depth;
static rt_base_t s_interrupt_state;
static bool s_interrupts_disabled;

static os_rtthread_task_t *task_find(rt_thread_t thread)
{
    os_rtthread_task_t *task;
    rt_base_t state = rt_hw_interrupt_disable();

    for (task = s_tasks; task != RT_NULL; task = task->next)
    {
        if (task->thread == thread) break;
    }
    rt_hw_interrupt_enable(state);
    return task;
}

static os_rtthread_task_t *task_resolve(osal_task_handle_t handle)
{
    return handle != NULL ? (os_rtthread_task_t *)handle
                          : task_find(rt_thread_self());
}

static void task_list_add(os_rtthread_task_t *task)
{
    rt_base_t state = rt_hw_interrupt_disable();
    task->next = s_tasks;
    s_tasks = task;
    rt_hw_interrupt_enable(state);
}

static void task_list_remove(os_rtthread_task_t *task)
{
    os_rtthread_task_t **cursor;
    rt_base_t state = rt_hw_interrupt_disable();

    for (cursor = &s_tasks; *cursor != RT_NULL; cursor = &(*cursor)->next)
    {
        if (*cursor == task)
        {
            *cursor = task->next;
            break;
        }
    }
    rt_hw_interrupt_enable(state);
}

static rt_uint8_t priority_to_native(osal_priority_t priority)
{
    uint32_t highest = RT_THREAD_PRIORITY_MAX > 2U
                     ? RT_THREAD_PRIORITY_MAX - 2U : 0U;
    uint32_t rank = priority < OSAL_PRIORITY_COUNT
                  ? (uint32_t)priority : (uint32_t)OSAL_PRIORITY_DEFAULT;

    return (rt_uint8_t)(highest -
        ((highest * rank) / (uint32_t)(OSAL_PRIORITY_COUNT - 1U)));
}

static void task_entry(void *argument)
{
    os_rtthread_task_t *task = argument;
    task->entry(task->argument);
}

int32_t os_task_create_impl(osal_task_internal_record_t *record)
{
    os_rtthread_task_t *task;

    task = rt_calloc(1U, sizeof(*task));
    if (task == RT_NULL) return OSAL_ERROR;
    task->entry = record->entry_function_pointer;
    task->argument = record->entry_arg;
    task->notification = rt_sem_create("osal_ntf", 0U, RT_IPC_FLAG_PRIO);
    if (task->notification == RT_NULL)
    {
        rt_free(task);
        return OSAL_ERROR;
    }
    task->thread = rt_thread_create(record->task_name, task_entry, task,
                                    (rt_uint32_t)record->stack_size,
                                    priority_to_native(record->priority), 10U);
    if (task->thread == RT_NULL)
    {
        rt_sem_delete(task->notification);
        rt_free(task);
        return OSAL_ERROR;
    }
    task_list_add(task);
    *record->task_handle = task;
    if (rt_thread_startup(task->thread) != RT_EOK)
    {
        task_list_remove(task);
        rt_thread_delete(task->thread);
        rt_sem_delete(task->notification);
        rt_free(task);
        *record->task_handle = NULL;
        return OSAL_ERROR;
    }
    return OSAL_SUCCESS;
}

void os_task_delete_impl(osal_task_handle_t handle)
{
    os_rtthread_task_t *task = task_resolve(handle);
    rt_thread_t thread;

    if (task == RT_NULL) return;
    thread = task->thread;
    task_list_remove(task);
    rt_sem_delete(task->notification);
    rt_free(task);
    (void)rt_thread_delete(thread);
}

void os_task_start_impl(void) { }

void os_task_suspend_impl(osal_task_handle_t handle)
{
    os_rtthread_task_t *task = task_resolve(handle);
    if (task != RT_NULL)
    {
        (void)rt_thread_suspend(task->thread);
        if (task->thread == rt_thread_self()) rt_schedule();
    }
}

int32_t os_task_resume_impl(osal_task_handle_t handle)
{
    os_rtthread_task_t *task = task_resolve(handle);
    if (os_rtthread_is_in_isr()) return OSAL_ERR_IN_ISR;
    if (task == RT_NULL) return OSAL_INVALID_POINTER;
    return rt_thread_resume(task->thread) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_task_resume_from_isr_impl(osal_task_handle_t handle)
{
    os_rtthread_task_t *task = task_resolve(handle);
    if (!os_rtthread_is_in_isr()) return OSAL_ERROR;
    if (task == RT_NULL) return OSAL_INVALID_POINTER;
    return rt_thread_resume(task->thread) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

void os_scheduler_lock_impl(void) { rt_enter_critical(); }
void os_scheduler_unlock_impl(void) { rt_exit_critical(); }

void os_task_delay_impl(uint32_t ticks)
{
    (void)rt_thread_delay((rt_tick_t)ticks);
}

void os_task_delay_ms_impl(uint32_t ms)
{
    (void)rt_thread_mdelay((rt_int32_t)ms);
}

void os_task_delay_until_impl(uint32_t *last_wake_time, uint32_t ticks)
{
    rt_tick_t wake;
    if (last_wake_time == NULL) return;
    wake = (rt_tick_t)*last_wake_time;
    (void)rt_thread_delay_until(&wake, (rt_tick_t)ticks);
    *last_wake_time = (uint32_t)wake;
}

void os_task_delay_until_ms_impl(uint32_t *last_wake_time, uint32_t ms)
{
    os_task_delay_until_impl(last_wake_time,
        (uint32_t)os_rtthread_ms_to_ticks(ms));
}

void os_enter_critical_impl(void)
{
    rt_base_t state = rt_hw_interrupt_disable();
    if (s_critical_depth++ == 0U) s_critical_state = state;
}

void os_exit_critical_impl(void)
{
    if (s_critical_depth > 0U && --s_critical_depth == 0U)
        rt_hw_interrupt_enable(s_critical_state);
}

osal_irq_state_t os_enter_critical_from_isr_impl(void)
{
    return (osal_irq_state_t)rt_hw_interrupt_disable();
}

void os_exit_critical_from_isr_impl(osal_irq_state_t state)
{
    rt_hw_interrupt_enable((rt_base_t)state);
}

int32_t os_port_yield_impl(void)
{
    if (os_rtthread_is_in_isr()) return OSAL_ERR_IN_ISR;
    return rt_thread_yield() == RT_EOK ? OSAL_SUCCESS : OSAL_ERROR;
}

void os_task_disable_interrupts_impl(void)
{
    if (!s_interrupts_disabled)
    {
        s_interrupt_state = rt_hw_interrupt_disable();
        s_interrupts_disabled = true;
    }
}

void os_task_enable_interrupts_impl(void)
{
    if (s_interrupts_disabled)
    {
        s_interrupts_disabled = false;
        rt_hw_interrupt_enable(s_interrupt_state);
    }
}

osal_tick_type_t os_task_get_tick_count_impl(void)
{
    return (osal_tick_type_t)rt_tick_get();
}

osal_time_ms_t os_time_get_ms_impl(void)
{
    return os_rtthread_ticks_to_ms(rt_tick_get());
}

size_t os_task_get_stack_high_water_mark_impl(osal_task_handle_t handle)
{
    (void)handle;
    return 0U;
}

osal_task_handle_t os_task_get_current_handle_impl(void)
{
    return task_find(rt_thread_self());
}

int32_t os_task_notify_give_impl(osal_task_handle_t handle)
{
    os_rtthread_task_t *task = task_resolve(handle);
    if (os_rtthread_is_in_isr()) return OSAL_ERR_IN_ISR;
    if (task == RT_NULL) return OSAL_INVALID_POINTER;
    return rt_sem_release(task->notification) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_task_notify_give_from_isr_impl(osal_task_handle_t handle)
{
    os_rtthread_task_t *task = task_resolve(handle);
    if (!os_rtthread_is_in_isr()) return OSAL_ERROR;
    if (task == RT_NULL) return OSAL_INVALID_POINTER;
    return rt_sem_release(task->notification) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

uint32_t os_task_notify_take_impl(uint32_t clear_on_exit,
                                  osal_time_ms_t timeout_ms)
{
    os_rtthread_task_t *task = task_find(rt_thread_self());
    uint32_t count = 0U;

    if (task == RT_NULL || os_rtthread_is_in_isr()) return 0U;
    if (rt_sem_take(task->notification,
                    os_rtthread_ms_to_ticks(timeout_ms)) != RT_EOK)
        return 0U;
    count = 1U;
    if (clear_on_exit != 0U)
    {
        while (rt_sem_trytake(task->notification) == RT_EOK) count++;
    }
    return count;
}

static void idle_hook(void)
{
    if (s_idle_hook != NULL) s_idle_hook(s_idle_hook_argument);
}

int32_t os_idle_hook_register_impl(osal_idle_hook_t hook, void *argument)
{
    s_idle_hook = hook;
    s_idle_hook_argument = argument;
    return rt_thread_idle_sethook(idle_hook) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_mutex_create_impl(osal_mutex_handle_t *handle)
{
    *handle = rt_mutex_create("osal_mtx", RT_IPC_FLAG_PRIO);
    return *handle != NULL ? OSAL_SUCCESS : OSAL_ERROR;
}

void os_mutex_delete_impl(osal_mutex_handle_t handle)
{
    (void)rt_mutex_delete((rt_mutex_t)handle);
}

int32_t os_mutex_give_impl(osal_mutex_handle_t handle)
{
    if (handle == NULL) return OSAL_INVALID_POINTER;
    return rt_mutex_release((rt_mutex_t)handle) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_mutex_take_impl(osal_mutex_handle_t handle,
                           osal_time_ms_t timeout_ms)
{
    rt_err_t result;
    if (handle == NULL) return OSAL_INVALID_POINTER;
    if (os_rtthread_is_in_isr()) return OSAL_ERR_IN_ISR;
    result = rt_mutex_take((rt_mutex_t)handle,
                           os_rtthread_ms_to_ticks(timeout_ms));
    return result == RT_EOK ? OSAL_SUCCESS : OSAL_ERROR_TIMEOUT;
}

int32_t os_sema_countings_create_impl(osal_sema_handle_t *handle,
                                      uint32_t max_count,
                                      uint32_t init_count)
{
    (void)max_count;
    *handle = rt_sem_create("osal_sem", init_count, RT_IPC_FLAG_PRIO);
    return *handle != NULL ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_sema_binary_create_impl(osal_sema_handle_t *handle)
{
    return os_sema_countings_create_impl(handle, 1U, 0U);
}

void os_sema_delete_impl(osal_sema_handle_t handle)
{
    (void)rt_sem_delete((rt_sem_t)handle);
}

int32_t os_sema_give_impl(osal_sema_handle_t handle)
{
    if (handle == NULL) return OSAL_INVALID_POINTER;
    return rt_sem_release((rt_sem_t)handle) == RT_EOK
         ? OSAL_SUCCESS : OSAL_SEM_FAILURE;
}

int32_t os_sema_give_from_isr_impl(osal_sema_handle_t handle)
{
    if (!os_rtthread_is_in_isr()) return OSAL_ERROR;
    return os_sema_give_impl(handle);
}

int32_t os_sema_take_impl(osal_sema_handle_t handle,
                          osal_time_ms_t timeout_ms)
{
    rt_err_t result;
    if (handle == NULL) return OSAL_INVALID_POINTER;
    if (os_rtthread_is_in_isr()) return OSAL_ERR_IN_ISR;
    result = rt_sem_take((rt_sem_t)handle,
                         os_rtthread_ms_to_ticks(timeout_ms));
    return result == RT_EOK ? OSAL_SUCCESS : OSAL_SEM_TIMEOUT;
}

int32_t os_queue_create_impl(size_t depth, size_t item_size,
                             osal_queue_handle_t *handle)
{
    os_rtthread_queue_t *queue = rt_calloc(1U, sizeof(*queue));
    if (queue == RT_NULL) return OSAL_ERROR;
    queue->queue = rt_mq_create("osal_mq", item_size, depth,
                                RT_IPC_FLAG_PRIO);
    if (queue->queue == RT_NULL)
    {
        rt_free(queue);
        return OSAL_ERROR;
    }
    queue->item_size = item_size;
    queue->depth = depth;
    *handle = queue;
    return OSAL_SUCCESS;
}

void os_queue_delete_impl(osal_queue_handle_t handle)
{
    os_rtthread_queue_t *queue = handle;
    if (queue == NULL) return;
    (void)rt_mq_delete(queue->queue);
    rt_free(queue);
}

static int32_t queue_send(os_rtthread_queue_t *queue, const void *data,
                          osal_time_ms_t timeout_ms)
{
    rt_tick_t start = rt_tick_get();
    rt_int32_t timeout = os_rtthread_ms_to_ticks(timeout_ms);

    do
    {
        if (rt_mq_send(queue->queue, data, queue->item_size) == RT_EOK)
            return OSAL_SUCCESS;
        if (timeout == 0) break;
        (void)rt_thread_delay(1U);
    } while (timeout == RT_WAITING_FOREVER ||
             (rt_int32_t)(rt_tick_get() - start) < timeout);
    return timeout_ms == OSAL_WAIT_NONE ? OSAL_QUEUE_FULL
                                        : OSAL_QUEUE_TIMEOUT;
}

int32_t os_queue_send_impl(osal_queue_handle_t handle, const void *data,
                           osal_time_ms_t timeout_ms)
{
    if (handle == NULL || data == NULL) return OSAL_INVALID_POINTER;
    if (os_rtthread_is_in_isr()) return OSAL_ERR_IN_ISR;
    return queue_send(handle, data, timeout_ms);
}

int32_t os_queue_receive_impl(osal_queue_handle_t handle, void *data,
                              osal_time_ms_t timeout_ms)
{
    os_rtthread_queue_t *queue = handle;
    rt_err_t result;
    if (queue == NULL || data == NULL) return OSAL_INVALID_POINTER;
    if (os_rtthread_is_in_isr()) return OSAL_ERR_IN_ISR;
    result = rt_mq_recv(queue->queue, data, queue->item_size,
                        os_rtthread_ms_to_ticks(timeout_ms));
    if (result == RT_EOK) return OSAL_SUCCESS;
    return timeout_ms == OSAL_WAIT_NONE ? OSAL_QUEUE_EMPTY
                                        : OSAL_QUEUE_TIMEOUT;
}

int32_t os_queue_send_from_isr_impl(osal_queue_handle_t handle,
                                    const void *data)
{
    os_rtthread_queue_t *queue = handle;
    if (!os_rtthread_is_in_isr()) return OSAL_ERROR;
    if (queue == NULL || data == NULL) return OSAL_INVALID_POINTER;
    return rt_mq_send(queue->queue, data, queue->item_size) == RT_EOK
         ? OSAL_SUCCESS : OSAL_QUEUE_FULL;
}

int32_t os_queue_receive_from_isr_impl(osal_queue_handle_t handle,
                                       void *data)
{
    (void)handle;
    (void)data;
    return OSAL_ERR_OPERATION_NOT_SUPPORTED;
}

int32_t os_queue_msg_waiting_impl(osal_queue_handle_t handle)
{
    os_rtthread_queue_t *queue = handle;
    return queue != NULL ? (int32_t)queue->queue->entry : 0;
}

size_t os_queue_spaces_available_impl(osal_queue_handle_t handle)
{
    os_rtthread_queue_t *queue = handle;
    size_t used;
    if (queue == NULL) return 0U;
    used = (size_t)queue->queue->entry;
    return used < queue->depth ? queue->depth - used : 0U;
}

static void timer_callback(void *argument)
{
    os_rtthread_timer_t *timer = argument;
    osal_timer_t *id = &timer->record->timer_id;
    if (id->func != NULL) id->func((osal_timer_handle_t)timer, id->arg);
}

int32_t os_timer_create_impl(osal_timer_handle_t *handle,
                             osal_timer_internal_record_t *record)
{
    os_rtthread_timer_t *timer = rt_calloc(1U, sizeof(*timer));
    rt_uint8_t flags;
    if (timer == RT_NULL) return OSAL_ERROR;
    timer->record = record;
    flags = record->auto_reload != 0U
          ? RT_TIMER_FLAG_PERIODIC : RT_TIMER_FLAG_ONE_SHOT;
#ifdef RT_TIMER_FLAG_SOFT_TIMER
    flags |= RT_TIMER_FLAG_SOFT_TIMER;
#endif
    timer->timer = rt_timer_create(record->timer_name, timer_callback, timer,
        (rt_tick_t)os_rtthread_ms_to_ticks(record->period_ms), flags);
    if (timer->timer == RT_NULL)
    {
        rt_free(timer);
        return OSAL_ERROR;
    }
    *handle = timer;
    record->timer_id.timer_handle = timer;
    return OSAL_SUCCESS;
}

int32_t os_timer_start_impl(osal_timer_handle_t handle,
                            osal_time_ms_t command_timeout_ms)
{
    (void)command_timeout_ms;
    return rt_timer_start(((os_rtthread_timer_t *)handle)->timer) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_timer_stop_impl(osal_timer_handle_t handle,
                           osal_time_ms_t command_timeout_ms)
{
    (void)command_timeout_ms;
    return rt_timer_stop(((os_rtthread_timer_t *)handle)->timer) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_timer_period_change_impl(osal_timer_handle_t handle,
                                    osal_time_ms_t new_period_ms,
                                    osal_time_ms_t command_timeout_ms)
{
    rt_tick_t ticks = (rt_tick_t)os_rtthread_ms_to_ticks(new_period_ms);
    (void)command_timeout_ms;
    return rt_timer_control(((os_rtthread_timer_t *)handle)->timer,
                            RT_TIMER_CTRL_SET_TIME, &ticks) == RT_EOK
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_timer_delete_impl(osal_timer_handle_t handle,
                             osal_time_ms_t command_timeout_ms)
{
    os_rtthread_timer_t *timer = handle;
    osal_timer_internal_record_t *record = timer->record;
    (void)command_timeout_ms;
    if (rt_timer_delete(timer->timer) != RT_EOK) return OSAL_ERROR;
    rt_free(timer);
    os_heap_free_impl(record);
    return OSAL_SUCCESS;
}

int32_t os_timer_reset_impl(osal_timer_handle_t handle,
                            osal_time_ms_t command_timeout_ms)
{
    (void)os_timer_stop_impl(handle, command_timeout_ms);
    return os_timer_start_impl(handle, command_timeout_ms);
}

osal_time_ms_t os_timer_period_get_impl(osal_timer_handle_t handle)
{
    rt_tick_t ticks = 0U;
    (void)rt_timer_control(((os_rtthread_timer_t *)handle)->timer,
                           RT_TIMER_CTRL_GET_TIME, &ticks);
    return os_rtthread_ticks_to_ms(ticks);
}

void *os_heap_malloc_impl(size_t size) { return rt_malloc(size); }
void os_heap_free_impl(void *ptr) { rt_free(ptr); }

size_t os_heap_get_free_size_impl(void)
{
    rt_size_t total = 0U, used = 0U, max_used = 0U;
    rt_memory_info(&total, &used, &max_used);
    return (size_t)(total - used);
}

size_t os_heap_get_minimum_ever_free_size_impl(void)
{
    rt_size_t total = 0U, used = 0U, max_used = 0U;
    rt_memory_info(&total, &used, &max_used);
    return (size_t)(total - max_used);
}

#endif /* OSAL_BACKEND == OSAL_BACKEND_RTTHREAD */
