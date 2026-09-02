#include "osal_internal_heap.h"
#include "os_freertos.h"

#if (OSAL_BACKEND == OSAL_BACKEND_FREERTOS)

void *os_heap_malloc_impl(size_t wanted_size)
{
    void *ptr = pvPortMalloc(wanted_size);
    return ptr;
}

void os_heap_free_impl(void *ptr)
{
    vPortFree(ptr);
}

size_t os_heap_get_free_size_impl(void)
{
    return xPortGetFreeHeapSize();
}

size_t os_heap_get_minimum_ever_free_size_impl(void)
{
    return xPortGetMinimumEverFreeHeapSize();
}

#endif /* OSAL_BACKEND */
