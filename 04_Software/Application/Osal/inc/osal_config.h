#ifndef __OSAL_CONFIG_H__
#define __OSAL_CONFIG_H__

#define OSAL_BACKEND_FREERTOS   (1U)
#define OSAL_BACKEND_ZEPHYR     (2U)
#define OSAL_BACKEND_RTTHREAD   (3U)

/* Select exactly one backend in the build configuration. */
#ifndef OSAL_BACKEND
#define OSAL_BACKEND OSAL_BACKEND_FREERTOS
#endif

#if (OSAL_BACKEND != OSAL_BACKEND_FREERTOS) && \
    (OSAL_BACKEND != OSAL_BACKEND_ZEPHYR) && \
    (OSAL_BACKEND != OSAL_BACKEND_RTTHREAD)
#error "Unsupported OSAL_BACKEND"
#endif

#if (OSAL_BACKEND == OSAL_BACKEND_FREERTOS)
#define OSAL_BACKEND_NAME "FreeRTOS"
#elif (OSAL_BACKEND == OSAL_BACKEND_ZEPHYR)
#define OSAL_BACKEND_NAME "Zephyr"
#else
#define OSAL_BACKEND_NAME "RT-Thread"
#endif

/* Portable object names include the trailing NUL in this limit. */
#define OSAL_NAME_MAX_LENGTH   (16U)


#endif // __OSAL_CONFIG_H__
