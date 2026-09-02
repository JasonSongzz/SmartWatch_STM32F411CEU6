#ifndef __OSAL_TIMER_H__
#define __OSAL_TIMER_H__

#include "common_types.h"

typedef void (*osal_timer_cb_function_t)(osal_timer_handle_t timer_handle, void *);

/* Timer callbacks always run in task/thread context on every backend. */
int32_t osal_timer_create(osal_timer_handle_t *timer_handle,
                          const char *timer_name,
                          osal_time_ms_t period_ms,
                          uint8_t auto_reload,
                          osal_timer_cb_function_t timer_cb,
                          void *arg);

int32_t osal_timer_start(osal_timer_handle_t timer_handle,
                         osal_time_ms_t command_timeout_ms);

int32_t osal_timer_stop(osal_timer_handle_t timer_handle,
                        osal_time_ms_t command_timeout_ms);

int32_t osal_timer_period_change(osal_timer_handle_t timer_handle,
                                 osal_time_ms_t new_period_ms,
                                 osal_time_ms_t command_timeout_ms);

int32_t osal_timer_delete(osal_timer_handle_t timer_handle,
                          osal_time_ms_t command_timeout_ms);

int32_t osal_timer_reset(osal_timer_handle_t timer_handle,
                         osal_time_ms_t command_timeout_ms);

osal_time_ms_t osal_timer_period_get(osal_timer_handle_t timer_handle);

#endif // __OSAL_TIMER_H__
