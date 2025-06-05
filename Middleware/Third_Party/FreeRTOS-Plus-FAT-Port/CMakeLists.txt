add_library(freertos_plus_fat_port STATIC)
target_sources(freertos_plus_fat_port
    PRIVATE
    ff_sddisk.c)

    # Configure your custom include path
target_include_directories(freertos_plus_fat_port
    PUBLIC
        FreeRTOS-Plus-FAT/include 
        "../../../Drivers/STM32H5xx_HAL_Driver/Inc"
        "../../../Drivers/CMSIS/Include"
        "../../../Drivers/CMSIS/Device/ST/STM32H5xx/Include")

set( FREERTOS_PLUS_FAT_PORT "A_CUSTOM_PORT" CACHE STRING "Configure custom port" )
set( FREERTOS_PLUS_FAT_FETCH_FREERTOS "OFF" CACHE BOOL "Fetch FreeRTOS Kernel" )

add_subdirectory( FreeRTOS-Plus-FAT )

target_link_libraries(freertos_plus_fat_port
    PUBLIC
        freertos_plus_fat_port_common
    PRIVATE
        freertos_kernel
        freertos_plus_fat)
