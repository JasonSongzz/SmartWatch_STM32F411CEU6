#ifndef __OSAL_QUEUE_H__
#define __OSAL_QUEUE_H__

#include "common_types.h"

int32_t osal_queue_create(size_t queue_depth, size_t data_size,osal_queue_handle_t *p_queue_handle);

void osal_queue_delete(osal_queue_handle_t queue_handle);

int32_t osal_queue_send(osal_queue_handle_t queue_handle, const void *data,
                        osal_time_ms_t timeout_ms);
int32_t osal_queue_receive(osal_queue_handle_t queue_handle, void *data,
                           osal_time_ms_t timeout_ms);

int32_t osal_queue_send_from_isr(osal_queue_handle_t queue_handle,
                                 const void *data);
int32_t osal_queue_receive_from_isr(osal_queue_handle_t queue_handle,
                                    void *data);

int32_t osal_queue_msg_waiting(osal_queue_handle_t queue_handle);

/** Number of free queue entries. Task context only; zero on invalid use. */
size_t osal_queue_spaces_available(osal_queue_handle_t queue_handle);



#endif // __OSAL_QUEUE_H__
