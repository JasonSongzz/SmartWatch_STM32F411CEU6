cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Application)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)
add_subdirectory(Osal)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Bsp/TempHumi/bsp_aht21_driver.c
    Bsp/TempHumi/bsp_sht30_driver.c
    Bsp/TempHumi/bsp_temp_humi_handler.c
    Bsp/Accel/bsp_mpu6050_driver.c
    Bsp/Accel/bsp_accel_handler.c
    Bsp/Display/bsp_st7789t3_driver.c
    Bsp/Display/bsp_display_handler.c
    Bsp/Touch/bsp_cst816t_driver.c
    Bsp/Touch/bsp_touch_handler.c
    Bsp/SPIFlash/bsp_spiflash_driver.c
    Service/UI/ui_test.c
    Platform/Wrapper/drv_adapter_accel.c
    Platform/Wrapper/drv_adapter_temphumi.c
    Platform/Wrapper/drv_adapter_display.c
    Platform/Wrapper/drv_adapter_touch.c
    Platform/Wrapper/drv_adapter_flash.c
    Platform/Port/drv_adapter_port_accel.c
    Platform/Port/drv_adapter_port_temphumi.c
    Platform/Port/drv_adapter_port_sensors.c
    Platform/Port/drv_adapter_port_display.c
    Platform/Port/drv_adapter_port_touch.c
    Platform/Port/drv_adapter_port_flash.c
    Platform/Port/lvgl_port_display.c
    Service/UI/lvgl_task.c
    Service/Storage/storage_service.c
    Middlewares/Third_Party/sfud/src/sfud.c
    Middlewares/Third_Party/sfud/src/sfud_sfdp.c
    Middlewares/Third_Party/flashDB/src/fdb.c
    Middlewares/Third_Party/flashDB/src/fdb_kvdb.c
    Middlewares/Third_Party/flashDB/src/fdb_tsdb.c
    Middlewares/Third_Party/flashDB/src/fdb_utils.c
    Middlewares/Third_Party/flashDB/port/fal/src/fal.c
    Middlewares/Third_Party/flashDB/port/fal/src/fal_flash.c
    Middlewares/Third_Party/flashDB/port/fal/src/fal_partition.c
    Core/Src/delay.c
    Core/Src/iic_hal.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Bsp/TempHumi
    Bsp/Accel
    Bsp/Display
    Bsp/Touch
    Bsp/SPIFlash
    Service/UI
    Service/Storage
    Platform/Wrapper
    Platform/Port
    Middlewares/Third_Party/sfud/inc
    Middlewares/Third_Party/flashDB/inc
    Middlewares/Third_Party/flashDB/port
    Middlewares/Third_Party/flashDB/port/fal/inc
    Middlewares/Third_Party/lvgl/include
    Middlewares/Third_Party/lvgl
)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/lvgl/lv_conf.h" CACHE PATH "LVGL configuration" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Disable LVGL examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Disable LVGL demos" FORCE)
add_subdirectory(Middlewares/Third_Party/lvgl)
target_include_directories(lvgl PRIVATE
    Core/Inc
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    lvgl
    osal

    # Add user defined libraries
)
