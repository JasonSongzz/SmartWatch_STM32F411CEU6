#include "osal_internal_heap.h"
#include "osal_internal_globaldefs.h"
//#include "app_log.h"



void *osal_heap_malloc(size_t wanted_size)
{
    return os_heap_malloc_impl(wanted_size);
}

void osal_heap_free(void *ptr)
{
    os_heap_free_impl(ptr);
}

size_t osal_heap_get_free_size(void)
{
    return os_heap_get_free_size_impl();
}

size_t osal_heap_get_minimum_ever_free_size(void)
{
    return os_heap_get_minimum_ever_free_size_impl();
}
