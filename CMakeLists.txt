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
set(CMAKE_PROJECT_NAME DCC_tester)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")
add_compile_definitions(STM32H563xx)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Include FetchContent module for dependency management
include(FetchContent)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

set(THIRD_PARTY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Middleware/Third_Party")

# Add the freertos_config for FreeRTOS-Kernel
add_library(freertos_config INTERFACE)

target_include_directories(freertos_config
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc"
)

# Select the heap port.  values between 1-4 will pick a heap.
set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

# Select the native compile PORT
set(FREERTOS_PORT "GCC_ARM_CM33_NTZ_NONSECURE" CACHE STRING "" FORCE)

# Adding the FreeRTOS subdirectory
add_subdirectory(${THIRD_PARTY_PATH}/FreeRTOS-Kernel)


target_compile_definitions(freertos_config INTERFACE ${definitions})
target_compile_options(freertos_config INTERFACE ${options})

set(FREERTOS_PLUS_TCP_NETWORK_IF "STM32" CACHE STRING "" FORCE)

# Adding the FreeRTOS-Plus-TCP subdirectory
add_subdirectory(${THIRD_PARTY_PATH}/FreeRTOS-Plus-TCP)

# Add include paths
target_include_directories(freertos_plus_tcp
    INTERFACE
    # Add user defined include paths
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Middleware/Third_Party/FreeRTOS-Kernel/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Middleware/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM33_NTZ/non_secure"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32H5xx_HAL_Driver/Inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32H5xx/Include"
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/App.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/FreeRTOS_CLI.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/cli_app.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    # Add user defined libraries
    freertos_kernel
    freertos_config
    freertos_plus_tcp
)

set_property(TARGET freertos_kernel PROPERTY C_STANDARD 90)
