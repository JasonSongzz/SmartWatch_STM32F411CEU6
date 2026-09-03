#include "osal_internal_heap.h"
#include "osal_internal_mutex.h"
#include "osal_internal_queue.h"
#include "osal_internal_sema.h"
#include "osal_internal_task.h"
#include "osal_internal_timer.h"
#include "os_zephyr.h"

#include <zephyr/sys/util.h>

#if (OSAL_BACKEND == OSAL_BACKEND_ZEPHYR)

typedef struct os_zephyr_task
{
    struct k_thread thread;
    k_thread_stack_t *stack;
    size_t stack_size;
    struct k_sem notification;
    osal_task_entry entry;
    void *argument;
    struct os_zephyr_task *next;
} os_zephyr_task_t;

typedef struct
{
    struct k_msgq queue;
    void *buffer;
    size_t item_size;
    size_t depth;
} os_zephyr_queue_t;

typedef struct
{
    struct k_work_delayable work;
    osal_timer_internal_record_t *record;
    osal_time_ms_t period_ms;
    bool auto_reload;
    bool active;
} os_zephyr_timer_t;

static os_zephyr_task_t *s_tasks;
static unsigned int s_critical_key;
static uint32_t s_critical_depth;
static unsigned int s_interrupt_key;
static bool s_interrupts_disabled;

static void task_list_add(os_zephyr_task_t *task)
{
    unsigned int key = irq_lock();
    task->next = s_tasks;
    s_tasks = task;
    irq_unlock(key);
}

static void task_list_remove(os_zephyr_task_t *task)
{
    os_zephyr_task_t **cursor;
    unsigned int key = irq_lock();

    for (cursor = &s_tasks; *cursor != NULL; cursor = &(*cursor)->next)
    {
        if (*cursor == task)
        {
            *cursor = task->next;
            break;
        }
    }
    irq_unlock(key);
}

static os_zephyr_task_t *task_find(k_tid_t thread)
{
    os_zephyr_task_t *task;
    unsigned int key = irq_lock();

    for (task = s_tasks; task != NULL; task = task->next)
    {
        if (&task->thread == thread) break;
    }
    irq_unlock(key);
    return task;
}

static os_zephyr_task_t *task_resolve(osal_task_handle_t handle)
{
    return handle != NULL ? (os_zephyr_task_t *)handle
                          : task_find(k_current_get());
}

static int priority_to_native(osal_priority_t priority)
{
    int lowest = CONFIG_NUM_PREEMPT_PRIORITIES - 1;
    int rank = priority < OSAL_PRIORITY_COUNT
             ? (int)priority : (int)OSAL_PRIORITY_DEFAULT;
    return lowest - ((lowest * rank) / ((int)OSAL_PRIORITY_COUNT - 1));
}

static void task_entry(void *p1, void *p2, void *p3)
{
    os_zephyr_task_t *task = p1;
    (void)p2;
    (void)p3;
    task->entry(task->argument);
}

int32_t os_task_create_impl(osal_task_internal_record_t *record)
{
    os_zephyr_task_t *task = k_calloc(1U, sizeof(*task));
    k_tid_t id;

    if (task == NULL) return OSAL_ERROR;
    task->stack = k_thread_stack_alloc(record->stack_size, 0);
    if (task->stack == NULL)
    {
        k_free(task);
        return OSAL_ERROR;
    }
    task->stack_size = record->stack_size;
    task->entry = record->entry_function_pointer;
    task->argument = record->entry_arg;
    k_sem_init(&task->notification, 0U, UINT_MAX);
    task_list_add(task);
    id = k_thread_create(&task->thread, task->stack, task->stack_size,
                         task_entry, task, NULL, NULL,
                         K_PRIO_PREEMPT(priority_to_native(record->priority)),
                         0U, K_NO_WAIT);
    if (id == NULL)
    {
        task_list_remove(task);
        (void)k_thread_stack_free(task->stack);
        k_free(task);
        return OSAL_ERROR;
    }
#if defined(CONFIG_THREAD_NAME)
    (void)k_thread_name_set(id, record->task_name);
#endif
    *record->task_handle = task;
    return OSAL_SUCCESS;
}

void os_task_delete_impl(osal_task_handle_t handle)
{
    os_zephyr_task_t *task = task_resolve(handle);
    bool current;

    if (task == NULL) return;
    current = &task->thread == k_current_get();
    task_list_remove(task);
    k_thread_abort(&task->thread);
    if (!current)
    {
        (void)k_thread_stack_free(task->stack);
        k_free(task);
    }
}

void os_task_start_impl(void) { }

void os_task_suspend_impl(osal_task_handle_t handle)
{
    os_zephyr_task_t *task = task_resolve(handle);
    if (task != NULL) k_thread_suspend(&task->thread);
}

int32_t os_task_resume_impl(osal_task_handle_t handle)
{
    os_zephyr_task_t *task = task_resolve(handle);
    if (k_is_in_isr()) return OSAL_ERR_IN_ISR;
    if (task == NULL) return OSAL_INVALID_POINTER;
    k_thread_resume(&task->thread);
    return OSAL_SUCCESS;
}

int32_t os_task_resume_from_isr_impl(osal_task_handle_t handle)
{
    os_zephyr_task_t *task = task_resolve(handle);
    if (!k_is_in_isr()) return OSAL_ERROR;
    if (task == NULL) return OSAL_INVALID_POINTER;
    k_thread_resume(&task->thread);
    return OSAL_SUCCESS;
}

void os_scheduler_lock_impl(void) { k_sched_lock(); }
void os_scheduler_unlock_impl(void) { k_sched_unlock(); }

void os_task_delay_impl(uint32_t ticks)
{
    (void)k_sleep(K_TICKS(ticks));
}

void os_task_delay_ms_impl(uint32_t ms)
{
    (void)k_msleep((int32_t)ms);
}

void os_task_delay_until_impl(uint32_t *last_wake_time, uint32_t ticks)
{
    uint32_t target;
    uint32_t now;
    int32_t remaining;

    if (last_wake_time == NULL) return;
    target = *last_wake_time + ticks;
    now = (uint32_t)k_uptime_ticks();
    remaining = (int32_t)(target - now);
    if (remaining > 0) (void)k_sleep(K_TICKS((uint32_t)remaining));
    *last_wake_time = target;
}

void os_task_delay_until_ms_impl(uint32_t *last_wake_time, uint32_t ms)
{
    os_task_delay_until_impl(last_wake_time,
        (uint32_t)k_ms_to_ticks_ceil32(ms));
}

void os_enter_critical_impl(void)
{
    unsigned int key = irq_lock();
    if (s_critical_depth++ == 0U) s_critical_key = key;
}

void os_exit_critical_impl(void)
{
    if (s_critical_depth > 0U && --s_critical_depth == 0U)
        irq_unlock(s_critical_key);
}

osal_irq_state_t os_enter_critical_from_isr_impl(void)
{
    return (osal_irq_state_t)irq_lock();
}

void os_exit_critical_from_isr_impl(osal_irq_state_t state)
{
    irq_unlock((unsigned int)state);
}

int32_t os_port_yield_impl(void)
{
    if (k_is_in_isr()) return OSAL_ERR_IN_ISR;
    k_yield();
    return OSAL_SUCCESS;
}

void os_task_disable_interrupts_impl(void)
{
    if (!s_interrupts_disabled)
    {
        s_interrupt_key = irq_lock();
        s_interrupts_disabled = true;
    }
}

void os_task_enable_interrupts_impl(void)
{
    if (s_interrupts_disabled)
    {
        s_interrupts_disabled = false;
        irq_unlock(s_interrupt_key);
    }
}

osal_tick_type_t os_task_get_tick_count_impl(void)
{
    return (osal_tick_type_t)k_uptime_ticks();
}

osal_time_ms_t os_time_get_ms_impl(void)
{
    return (osal_time_ms_t)k_uptime_get_32();
}

size_t os_task_get_stack_high_water_mark_impl(osal_task_handle_t handle)
{
    os_zephyr_task_t *task = task_resolve(handle);
    size_t unused = 0U;
#if defined(CONFIG_THREAD_STACK_INFO) && defined(CONFIG_INIT_STACKS)
    if (task != NULL && k_thread_stack_space_get(&task->thread,
                                                  &unused) != 0)
        unused = 0U;
#else
    (void)task;
#endif
    return unused;
}

osal_task_handle_t os_task_get_current_handle_impl(void)
{
    return task_find(k_current_get());
}

int32_t os_task_notify_give_impl(osal_task_handle_t handle)
{
    os_zephyr_task_t *task = task_resolve(handle);
    if (k_is_in_isr()) return OSAL_ERR_IN_ISR;
    if (task == NULL) return OSAL_INVALID_POINTER;
    k_sem_give(&task->notification);
    return OSAL_SUCCESS;
}

int32_t os_task_notify_give_from_isr_impl(osal_task_handle_t handle)
{
    os_zephyr_task_t *task = task_resolve(handle);
    if (!k_is_in_isr()) return OSAL_ERROR;
    if (task == NULL) return OSAL_INVALID_POINTER;
    k_sem_give(&task->notification);
    return OSAL_SUCCESS;
}

uint32_t os_task_notify_take_impl(uint32_t clear_on_exit,
                                  osal_time_ms_t timeout_ms)
{
    os_zephyr_task_t *task = task_find(k_current_get());
    uint32_t count = 0U;

    if (task == NULL || k_is_in_isr()) return 0U;
    if (k_sem_take(&task->notification,
                   os_zephyr_timeout(timeout_ms)) != 0)
        return 0U;
    count = 1U;
    if (clear_on_exit != 0U)
    {
        while (k_sem_take(&task->notification, K_NO_WAIT) == 0) count++;
    }
    return count;
}

int32_t os_idle_hook_register_impl(osal_idle_hook_t hook, void *argument)
{
    (void)hook;
    (void)argument;
    return OSAL_ERR_NOT_IMPLEMENTED;
}

int32_t os_mutex_create_impl(osal_mutex_handle_t *handle)
{
    struct k_mutex *mutex = k_malloc(sizeof(*mutex));
    if (mutex == NULL) return OSAL_ERROR;
    k_mutex_init(mutex);
    *handle = mutex;
    return OSAL_SUCCESS;
}

void os_mutex_delete_impl(osal_mutex_handle_t handle)
{
    k_free(handle);
}

int32_t os_mutex_give_impl(osal_mutex_handle_t handle)
{
    if (handle == NULL) return OSAL_INVALID_POINTER;
    return k_mutex_unlock((struct k_mutex *)handle) == 0
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_mutex_take_impl(osal_mutex_handle_t handle,
                           osal_time_ms_t timeout_ms)
{
    if (handle == NULL) return OSAL_INVALID_POINTER;
    if (k_is_in_isr()) return OSAL_ERR_IN_ISR;
    return k_mutex_lock((struct k_mutex *)handle,
                        os_zephyr_timeout(timeout_ms)) == 0
         ? OSAL_SUCCESS : OSAL_ERROR_TIMEOUT;
}

int32_t os_sema_countings_create_impl(osal_sema_handle_t *handle,
                                      uint32_t max_count,
                                      uint32_t init_count)
{
    struct k_sem *semaphore = k_malloc(sizeof(*semaphore));
    if (semaphore == NULL) return OSAL_ERROR;
    if (k_sem_init(semaphore, init_count, max_count) != 0)
    {
        k_free(semaphore);
        return OSAL_ERROR;
    }
    *handle = semaphore;
    return OSAL_SUCCESS;
}

int32_t os_sema_binary_create_impl(osal_sema_handle_t *handle)
{
    return os_sema_countings_create_impl(handle, 1U, 0U);
}

void os_sema_delete_impl(osal_sema_handle_t handle)
{
    k_free(handle);
}

int32_t os_sema_give_impl(osal_sema_handle_t handle)
{
    if (handle == NULL) return OSAL_INVALID_POINTER;
    k_sem_give((struct k_sem *)handle);
    return OSAL_SUCCESS;
}

int32_t os_sema_give_from_isr_impl(osal_sema_handle_t handle)
{
    if (!k_is_in_isr()) return OSAL_ERROR;
    return os_sema_give_impl(handle);
}

int32_t os_sema_take_impl(osal_sema_handle_t handle,
                          osal_time_ms_t timeout_ms)
{
    if (handle == NULL) return OSAL_INVALID_POINTER;
    if (k_is_in_isr()) return OSAL_ERR_IN_ISR;
    return k_sem_take((struct k_sem *)handle,
                      os_zephyr_timeout(timeout_ms)) == 0
         ? OSAL_SUCCESS : OSAL_SEM_TIMEOUT;
}

int32_t os_queue_create_impl(size_t depth, size_t item_size,
                             osal_queue_handle_t *handle)
{
    os_zephyr_queue_t *queue = k_calloc(1U, sizeof(*queue));
    if (queue == NULL) return OSAL_ERROR;
    queue->buffer = k_malloc(depth * item_size);
    if (queue->buffer == NULL)
    {
        k_free(queue);
        return OSAL_ERROR;
    }
    queue->item_size = item_size;
    queue->depth = depth;
    k_msgq_init(&queue->queue, queue->buffer, item_size, depth);
    *handle = queue;
    return OSAL_SUCCESS;
}

void os_queue_delete_impl(osal_queue_handle_t handle)
{
    os_zephyr_queue_t *queue = handle;
    if (queue == NULL) return;
    k_msgq_purge(&queue->queue);
    k_free(queue->buffer);
    k_free(queue);
}

int32_t os_queue_send_impl(osal_queue_handle_t handle, const void *data,
                           osal_time_ms_t timeout_ms)
{
    os_zephyr_queue_t *queue = handle;
    if (queue == NULL || data == NULL) return OSAL_INVALID_POINTER;
    if (k_is_in_isr()) return OSAL_ERR_IN_ISR;
    if (k_msgq_put(&queue->queue, data,
                   os_zephyr_timeout(timeout_ms)) == 0)
        return OSAL_SUCCESS;
    return timeout_ms == OSAL_WAIT_NONE ? OSAL_QUEUE_FULL
                                        : OSAL_QUEUE_TIMEOUT;
}

int32_t os_queue_receive_impl(osal_queue_handle_t handle, void *data,
                              osal_time_ms_t timeout_ms)
{
    os_zephyr_queue_t *queue = handle;
    if (queue == NULL || data == NULL) return OSAL_INVALID_POINTER;
    if (k_is_in_isr()) return OSAL_ERR_IN_ISR;
    if (k_msgq_get(&queue->queue, data,
                   os_zephyr_timeout(timeout_ms)) == 0)
        return OSAL_SUCCESS;
    return timeout_ms == OSAL_WAIT_NONE ? OSAL_QUEUE_EMPTY
                                        : OSAL_QUEUE_TIMEOUT;
}

int32_t os_queue_send_from_isr_impl(osal_queue_handle_t handle,
                                    const void *data)
{
    os_zephyr_queue_t *queue = handle;
    if (!k_is_in_isr()) return OSAL_ERROR;
    if (queue == NULL || data == NULL) return OSAL_INVALID_POINTER;
    return k_msgq_put(&queue->queue, data, K_NO_WAIT) == 0
         ? OSAL_SUCCESS : OSAL_QUEUE_FULL;
}

int32_t os_queue_receive_from_isr_impl(osal_queue_handle_t handle,
                                       void *data)
{
    os_zephyr_queue_t *queue = handle;
    if (!k_is_in_isr()) return OSAL_ERROR;
    if (queue == NULL || data == NULL) return OSAL_INVALID_POINTER;
    return k_msgq_get(&queue->queue, data, K_NO_WAIT) == 0
         ? OSAL_SUCCESS : OSAL_QUEUE_EMPTY;
}

int32_t os_queue_msg_waiting_impl(osal_queue_handle_t handle)
{
    os_zephyr_queue_t *queue = handle;
    return queue != NULL ? (int32_t)k_msgq_num_used_get(&queue->queue) : 0;
}

size_t os_queue_spaces_available_impl(osal_queue_handle_t handle)
{
    os_zephyr_queue_t *queue = handle;
    return queue != NULL ? (size_t)k_msgq_num_free_get(&queue->queue) : 0U;
}

static void timer_callback(struct k_work *work)
{
    os_zephyr_timer_t *timer =
        CONTAINER_OF(work, os_zephyr_timer_t, work.work);
    osal_timer_t *id = &timer->record->timer_id;

    if (!timer->active) return;
    if (id->func != NULL) id->func((osal_timer_handle_t)timer, id->arg);
    if (timer->auto_reload && timer->active)
        (void)k_work_reschedule(&timer->work, K_MSEC(timer->period_ms));
    else
        timer->active = false;
}

int32_t os_timer_create_impl(osal_timer_handle_t *handle,
                             osal_timer_internal_record_t *record)
{
    os_zephyr_timer_t *timer = k_calloc(1U, sizeof(*timer));
    if (timer == NULL) return OSAL_ERROR;
    timer->record = record;
    timer->period_ms = record->period_ms;
    timer->auto_reload = record->auto_reload != 0U;
    k_work_init_delayable(&timer->work, timer_callback);
    *handle = timer;
    record->timer_id.timer_handle = timer;
    return OSAL_SUCCESS;
}

int32_t os_timer_start_impl(osal_timer_handle_t handle,
                            osal_time_ms_t command_timeout_ms)
{
    os_zephyr_timer_t *timer = handle;
    (void)command_timeout_ms;
    timer->active = true;
    return k_work_reschedule(&timer->work, K_MSEC(timer->period_ms)) >= 0
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_timer_stop_impl(osal_timer_handle_t handle,
                           osal_time_ms_t command_timeout_ms)
{
    os_zephyr_timer_t *timer = handle;
    (void)command_timeout_ms;
    timer->active = false;
    return k_work_cancel_delayable(&timer->work) >= 0
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_timer_period_change_impl(osal_timer_handle_t handle,
                                    osal_time_ms_t new_period_ms,
                                    osal_time_ms_t command_timeout_ms)
{
    os_zephyr_timer_t *timer = handle;
    (void)command_timeout_ms;
    timer->period_ms = new_period_ms;
    if (!timer->active) return OSAL_SUCCESS;
    return k_work_reschedule(&timer->work, K_MSEC(new_period_ms)) >= 0
         ? OSAL_SUCCESS : OSAL_ERROR;
}

int32_t os_timer_delete_impl(osal_timer_handle_t handle,
                             osal_time_ms_t command_timeout_ms)
{
    os_zephyr_timer_t *timer = handle;
    struct k_work_sync sync;
    osal_timer_internal_record_t *record = timer->record;
    (void)command_timeout_ms;
    timer->active = false;
    (void)k_work_cancel_delayable_sync(&timer->work, &sync);
    k_free(timer);
    os_heap_free_impl(record);
    return OSAL_SUCCESS;
}

int32_t os_timer_reset_impl(osal_timer_handle_t handle,
                            osal_time_ms_t command_timeout_ms)
{
    return os_timer_start_impl(handle, command_timeout_ms);
}

osal_time_ms_t os_timer_period_get_impl(osal_timer_handle_t handle)
{
    return ((os_zephyr_timer_t *)handle)->period_ms;
}

void *os_heap_malloc_impl(size_t size) { return k_malloc(size); }
void os_heap_free_impl(void *ptr) { k_free(ptr); }
size_t os_heap_get_free_size_impl(void) { return 0U; }
size_t os_heap_get_minimum_ever_free_size_impl(void) { return 0U; }

#endif /* OSAL_BACKEND == OSAL_BACKEND_ZEPHYR */
