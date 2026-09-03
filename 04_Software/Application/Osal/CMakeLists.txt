cmake_minimum_required(VERSION 3.22)

set(OSAL_BACKEND "FreeRTOS" CACHE STRING
    "OSAL backend: FreeRTOS, RTThread, or Zephyr")
set_property(CACHE OSAL_BACKEND PROPERTY STRINGS FreeRTOS RTThread Zephyr)
set(OSAL_NATIVE_TARGET "" CACHE STRING
    "Optional native RTOS CMake target used by RTThread or Zephyr")
set(OSAL_NATIVE_INCLUDE_DIRS "" CACHE STRING
    "Optional native RTOS include directories used by RTThread or Zephyr")

set(OSAL_SHARED_SOURCES
    shared/src/osal_heap.c
    shared/src/osal_mutex.c
    shared/src/osal_queue.c
    shared/src/osal_sema.c
    shared/src/osal_task.c
    shared/src/osal_timer.c
)

if(OSAL_BACKEND STREQUAL "FreeRTOS")
    set(OSAL_BACKEND_VALUE 1)
    set(OSAL_BACKEND_SOURCES
        FreeRTOS/src/os_impl_heap.c
        FreeRTOS/src/os_impl_mutex.c
        FreeRTOS/src/os_impl_queue.c
        FreeRTOS/src/os_impl_sema.c
        FreeRTOS/src/os_impl_task.c
        FreeRTOS/src/os_impl_timer.c
    )
    set(OSAL_BACKEND_INCLUDE_DIR FreeRTOS/inc)
elseif(OSAL_BACKEND STREQUAL "Zephyr")
    set(OSAL_BACKEND_VALUE 2)
    set(OSAL_BACKEND_SOURCES Zephyr/src/os_impl.c)
    set(OSAL_BACKEND_INCLUDE_DIR Zephyr/inc)
elseif(OSAL_BACKEND STREQUAL "RTThread")
    set(OSAL_BACKEND_VALUE 3)
    set(OSAL_BACKEND_SOURCES RTThread/src/os_impl.c)
    set(OSAL_BACKEND_INCLUDE_DIR RTThread/inc)
else()
    message(FATAL_ERROR
        "Unsupported OSAL_BACKEND='${OSAL_BACKEND}'. "
        "Choose FreeRTOS, RTThread, or Zephyr.")
endif()

add_library(osal STATIC ${OSAL_SHARED_SOURCES} ${OSAL_BACKEND_SOURCES})
target_include_directories(osal
    PUBLIC inc
    PRIVATE shared/inc ${OSAL_BACKEND_INCLUDE_DIR}
            ${OSAL_NATIVE_INCLUDE_DIRS}
)
target_compile_definitions(osal PUBLIC OSAL_BACKEND=${OSAL_BACKEND_VALUE})

if(OSAL_BACKEND STREQUAL "FreeRTOS")
    # The current STM32CubeMX project exports FreeRTOS/CMSIS include paths.
    target_link_libraries(osal PUBLIC stm32cubemx)
elseif(OSAL_NATIVE_TARGET)
    if(NOT TARGET ${OSAL_NATIVE_TARGET})
        message(FATAL_ERROR
            "OSAL_NATIVE_TARGET '${OSAL_NATIVE_TARGET}' does not exist")
    endif()
    target_link_libraries(osal PUBLIC ${OSAL_NATIVE_TARGET})
endif()

message(STATUS "OSAL backend: ${OSAL_BACKEND}")
