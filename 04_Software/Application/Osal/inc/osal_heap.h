#ifndef __OSAL_HEAP_H__
#define __OSAL_HEAP_H__

#include "common_types.h"

void* osal_heap_malloc(size_t wanted_size);

void osal_heap_free(void *ptr);

size_t osal_heap_get_free_size(void);

size_t osal_heap_get_minimum_ever_free_size(void);

#endif // __OSAL_HEAP_H__
