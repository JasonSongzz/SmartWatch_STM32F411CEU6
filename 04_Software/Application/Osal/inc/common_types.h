#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Public blocking durations are always milliseconds. Native kernel ticks are
 * kept separate and only used as opaque periodic-delay state. */
typedef uint32_t osal_time_ms_t;
typedef uint32_t osal_tick_type_t;

#define OSAL_WAIT_NONE       ((osal_time_ms_t)0U)
#define OSAL_WAIT_FOREVER    ((osal_time_ms_t)UINT32_MAX)

/* Compatibility for existing users; prefer OSAL_WAIT_FOREVER. */
#define OSAL_MAX_DELAY       OSAL_WAIT_FOREVER

typedef long osal_base_type_t;
typedef void *osal_task_handle_t;
typedef uint32_t osal_irq_state_t;

typedef void *osal_sema_handle_t;
typedef void *osal_mutex_handle_t;
typedef void *osal_queue_handle_t;
typedef void *osal_timer_handle_t;

#define OSAL_TRUE  ((osal_base_type_t)1)
#define OSAL_FALSE ((osal_base_type_t)0)

#endif // __COMMON_TYPES_H__
