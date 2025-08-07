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


add_library(freertos_kernel STATIC
    Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    Middlewares/Third_Party/FreeRTOS/Source/queue.c
    Middlewares/Third_Party/FreeRTOS/Source/list.c
    Middlewares/Third_Party/FreeRTOS/Source/timers.c
    Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
)

target_include_directories(freertos_kernel PUBLIC
    ${CMAKE_SOURCE_DIR}/Core/Inc
    Middlewares/Third_Party/FreeRTOS/Source/include
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/
)

add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE ${PROJECT_SOURCE_DIR}/Core/Inc)

add_library(freertos_plus_tcp_utilities INTERFACE)
target_sources(freertos_plus_tcp_utilities INTERFACE
    MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP/FreeRTOS_TCP_Utils.c
)
target_include_directories(freertos_plus_tcp_utilities INTERFACE MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP/include)



include(FetchContent)

FetchContent_Declare(
  DCC
  GIT_REPOSITORY "https://github.com/ZIMO-Elektronik/DCC"
  GIT_TAG v0.41.1)

FetchContent_MakeAvailable(DCC)


set(MYTHIRD_PARTY_PATH "${CMAKE_SOURCE_DIR}/MyMiddlewares/Third_Party")
set(THIRD_PARTY_PATH "${CMAKE_SOURCE_DIR}/Middlewares/Third_Party")

# Add FatFs library
add_subdirectory(MyMiddlewares/Third_Party/FatFs)

# Add FreeRTOS-Plus-TCP library
add_subdirectory(MyMiddlewares/Third_Party/FreeRTOS-Plus-TCP)
#target_compile_definitions(freertos_plus_tcp PUBLIC FREERTOS_TCP_BUFFER_ALLOCATION=2)
target_include_directories(freertos_plus_tcp PUBLIC
    ${THIRD_PARTY_PATH}/FreeRTOS/Source/include
    ${CMAKE_SOURCE_DIR}/TCPIP/Target
    ${PROJECT_SOURCE_DIR}/Core/Inc
)

target_include_directories(freertos_plus_tcp_port PUBLIC
    ${THIRD_PARTY_PATH}/FreeRTOS/Source/include
    ${CMAKE_SOURCE_DIR}/TCPIP/Target
    ${PROJECT_SOURCE_DIR}/Core/Inc
)

add_library(freertos_plus_tcp_network_if STATIC
    TCPIP/Target/NetworkInterface.c
)
#add_subdirectory(TCPIP/Target)

target_include_directories(freertos_plus_tcp_network_if PUBLIC
    ${MYTHIRD_PARTY_PATH}/FreeRTOS-Plus-TCP/portable/Compiler/GCC
    ${THIRD_PARTY_PATH}/FreeRTOS/Source/include
    ${MYTHIRD_PARTY_PATH}/FreeRTOS-Plus-TCP/include
    ${CMAKE_SOURCE_DIR}/TCPIP/Target
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32H5xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32H5xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_compile_definitions(freertos_plus_tcp_network_if PRIVATE STM32H563xx)

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
#    TCPIP/Target/NetworkInterface.c

)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/FATFS/App
    ${CMAKE_SOURCE_DIR}/FATFS/Target
    ${CMAKE_SOURCE_DIR}/TCPIP/Target
    ${THIRD_PARTY_PATH}/FreeRTOS/Source/include
    ${MYTHIRD_PARTY_PATH}/FatFs/src
    ${MYTHIRD_PARTY_PATH}/FreeRTOS-Plus-TCP/include
    ${MYTHIRD_PARTY_PATH}/FreeRTOS-Plus-TCP/portable/Compiler/GCC
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
    freertos_plus_tcp_port # for pack_struct_start.h
    freertos_plus_tcp_network_if # for NetworkInterface.c
    freertos_plus_tcp_utilities
)
