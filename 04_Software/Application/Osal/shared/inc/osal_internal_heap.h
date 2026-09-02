#ifndef __OSAL_INTERNAL_HEAP_H__
#define __OSAL_INTERNAL_HEAP_H__

#include "osal_heap.h"

void *os_heap_malloc_impl(size_t wanted_size);

void os_heap_free_impl(void *ptr);

size_t os_heap_get_free_size_impl(void);

size_t os_heap_get_minimum_ever_free_size_impl(void);

#endif // __OSAL_INTERNAL_HEAP_H__
