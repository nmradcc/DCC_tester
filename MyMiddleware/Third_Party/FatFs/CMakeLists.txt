cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)

set(FAT_Defines_Syms 
	USE_HAL_DRIVER 
	STM32L476xx
    $<$<CONFIG:Debug>:DEBUG>
)

set(FatFs_Include_Dirs
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Middleware/Third_Party/FatFs/src
    ${CMAKE_SOURCE_DIR}/Drivers/STM32L4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32L4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32L4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

set(fatfs_Source
    ${CMAKE_SOURCE_DIR}/Middleware/Third_Party/FatFs/src/diskio.c
    ${CMAKE_SOURCE_DIR}/Middleware/Third_Party/FatFs/src/ff.c
    ${CMAKE_SOURCE_DIR}/Middleware/Third_Party/FatFs/src/ff_gen_drv.c
    ${CMAKE_SOURCE_DIR}/Middleware/Third_Party/FatFs/src/option/syscall.c
)

add_library(fatfs STATIC 
    ${fatfs_Source}
)

target_compile_definitions(fatfs PUBLIC ${FAT_Defines_Syms})
target_include_directories(fatfs PUBLIC ${FatFs_Include_Dirs})

