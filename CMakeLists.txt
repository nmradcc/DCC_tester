cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    @ONLY
)

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME DCC_tester)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

add_compile_definitions(STM32H563xx)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add FreeRTOS library
file(GLOB FREERTOS_KERNEL_SOURCES
    Third_Party/FreeRTOS/Source/*.c
    Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/*.c
)

add_library(freertos_kernel STATIC ${FREERTOS_KERNEL_SOURCES})
target_include_directories(freertos_kernel PUBLIC
    Core/Inc
    Third_Party/FreeRTOS/Source/include
    Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/
)

# Add FreeRTOS config library
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE Core/Inc)

# Add RTOS2 library
file(GLOB RTOS2_SOURCES
    Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    Third_Party/FreeRTOS/Source/*.c
    Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/*.c
    Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
)

add_library(RTOS2 OBJECT)
target_sources(RTOS2 PRIVATE ${RTOS2_SOURCES})
target_include_directories(RTOS2 PRIVATE
    Core/Inc
    Third_Party/FreeRTOS/Source/include
    Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure
    Third_Party/CMSIS/RTOS2/include
    Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32H5xx/Include
)

# Add FatFs library
file(GLOB FATFS_SOURCES
    Third_Party/FatFs/source/*.c
    Third_Party/FatFs/source/drivers/sd/sd_diskio_dma_rtos.c
)

add_library(fatfs STATIC ${FATFS_SOURCES})
target_include_directories(fatfs PUBLIC
    Core/Inc
    Third_Party/FatFs/source
    Third_Party/FatFs/source/drivers/sd
    Third_Party/CMSIS/RTOS2/include
    Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    Drivers/STM32H5xx_HAL_Driver/Inc
    Drivers/CMSIS/Include
    Drivers/CMSIS/Device/ST/STM32H5xx/Include
    FATFS/Target
)
#add_subdirectory(MyMiddlewares/Third_Party/FatFs)


include(FetchContent)

FetchContent_Declare(
  DCC
  GIT_REPOSITORY "https://github.com/ZIMO-Elektronik/DCC"
  GIT_TAG v0.41.1)

FetchContent_MakeAvailable(DCC)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    newlib_lock_glue.c
    Core/Src/app_freertos.c
    Core/Src/cli_app.c
    Core/Src/command_station.cpp
    Core/Src/decoder.cpp
    FATFS/App/fatfs.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    Core/Inc
    FATFS/App
    FATFS/Target
    Third_Party/FreeRTOS/Source/include
    Third_Party/CMSIS/RTOS2/include
    Third_Party/FatFs/src
)


# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    freertos_kernel
    freertos_config
    RTOS2
    # Add user defined libraries
    DCC::DCC
    fatfs
)
