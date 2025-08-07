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


# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add FreeRTOS library
file(GLOB FREERTOS_KERNEL_SOURCES
    Middlewares/Third_Party/FreeRTOS/Source/*.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/*.c
)

add_library(freertos_kernel STATIC ${FREERTOS_KERNEL_SOURCES})
target_include_directories(freertos_kernel PUBLIC
    Core/Inc
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/
)

# Add FreeRTOS config library
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE Core/Inc)

# Add FreeRTOS-Plus-TCP library
add_subdirectory(MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP)
target_include_directories(freertos_plus_tcp PUBLIC
    Middlewares/Third_Party/FreeRTOS/Source/include
    TCPIP/Target
    Core/Inc
)

#target_include_directories(freertos_plus_tcp_port PUBLIC
#    Middlewares/Third_Party/FreeRTOS/Source/include
#    TCPIP/Target
#    Core/Inc
#)
#target_compile_definitions(freertos_plus_tcp_port PUBLIC FREERTOS_PLUS_TCP_BUFFER_ALLOCATION=2)

add_library(freertos_plus_tcp_network_if STATIC
    TCPIP/Target/NetworkInterface.c
)

target_include_directories(freertos_plus_tcp_network_if PUBLIC
    MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP/portable/Compiler/GCC
    MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP/include
    Middlewares/Third_Party/FreeRTOS/Source/include
    TCPIP/Target
    Core/Inc
    Drivers/STM32H5xx_HAL_Driver/Inc
    Drivers/CMSIS/Device/ST/STM32H5xx/Include
    Drivers/CMSIS/Include
)
target_compile_definitions(freertos_plus_tcp_network_if PRIVATE STM32H563xx)

add_library(freertos_plus_tcp_utilities INTERFACE MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP/FreeRTOS_TCP_Utils.c)
target_include_directories(freertos_plus_tcp_utilities INTERFACE MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP/include)


# Add FatFs library
add_subdirectory(MyMiddlewares/Third_Party/FatFs)


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
    Core/Src/cli_app.c
    Core/Src/command_station.cpp
    Core/Src/decoder.cpp
    FATFS/App/fatfs.c
    FATFS/Target/sd_diskio_dma_rtos.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    Core/Inc
    FATFS/App
    FATFS/Target
    TCPIP/Target
    Middlewares/Third_Party/FreeRTOS/Source/include
    MyMiddlewares/Third_Party/FatFs/src
    MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP/include
    MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP/portable/Compiler/GCC
)


# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    DCC::DCC
    fatfs
    freertos_kernel
    freertos_config
    freertos_plus_tcp
    freertos_plus_tcp_utilities
    freertos_plus_tcp_port # for pack_struct_start.h
    freertos_plus_tcp_network_if # for NetworkInterface.c
)
