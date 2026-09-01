/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 */
 
#include <elog.h>
#include <stdbool.h>
#include <stdio.h>
#include "main.h"
#include "Date.h"
#include "SEGGER_RTT.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/* 校时后：日期时间+毫秒；校时前：tick — elog.c 外层再包一层 [...] */
#define ELOG_TIME_BUF_SIZE 32U
static char s_elog_time_buf[ELOG_TIME_BUF_SIZE];
static SemaphoreHandle_t s_elog_mutex;

extern bool g_elog_flash_write_enabled;

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void) {
    ElogErrCode result = ELOG_NO_ERR;
    s_elog_mutex = xSemaphoreCreateRecursiveMutex();
    /* Do not call SEGGER_RTT_Init(): it memset's the whole RTT control block and
       wipes SystemView channels if SEGGER_SYSVIEW_Conf() ran first. RTT is
       already initialized; first SEGGER_RTT_Write triggers lazy init if needed. */
    return result;
}

/**
 * EasyLogger port deinitialize
 *
 */
void elog_port_deinit(void) {

    /* add your code here */

}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size) {
    
    /* add your code here */
    SEGGER_RTT_Write(0, log, size);
}

/**
 * output lock
 */
void elog_port_output_lock(void) {
    /* Formatting uses shared EasyLogger buffers, so task-level output is serialized. */
    configASSERT(__get_IPSR() == 0U);
    if (s_elog_mutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        (void)xSemaphoreTakeRecursive(s_elog_mutex, portMAX_DELAY);
    }
}

/**
 * output unlock
 */
void elog_port_output_unlock(void) {
    configASSERT(__get_IPSR() == 0U);
    if (s_elog_mutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        (void)xSemaphoreGiveRecursive(s_elog_mutex);
    }
}

/**
 * get current time interface
 *
 * @return current time
 */
const char *elog_port_get_time(void) {
    uint32_t uptime_seconds = HAL_GetTick() / 1000U;
    uint32_t uptime_hours = uptime_seconds / 3600U;
    uint32_t uptime_minutes = (uptime_seconds % 3600U) / 60U;
    uint32_t uptime_remaining_seconds = uptime_seconds % 60U;

    (void)snprintf(s_elog_time_buf, sizeof(s_elog_time_buf),
                   "Uptime:%02lu:%02lu:%02lu",
                   (unsigned long)uptime_hours,
                   (unsigned long)uptime_minutes,
                   (unsigned long)uptime_remaining_seconds);
    return s_elog_time_buf;
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void) {
    return "";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void) {
    return "";
}
