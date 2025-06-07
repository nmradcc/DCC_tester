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
#??add_compile_definitions(STM32H563xx)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

#??# Include FetchContent module for dependency management
#??include(FetchContent)
#??
# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add FatFs port library
add_subdirectory(Middleware/Third_Party/FatFs)

set(THIRD_PARTY_PATH "${CMAKE_SOURCE_DIR}/Middleware/Third_Party")

set(FREERTOS_PLUS_TCP_NETWORK_IF "STM32" CACHE STRING "" FORCE)

# Adding the FreeRTOS-Plus-TCP subdirectory
#add_subdirectory(${THIRD_PARTY_PATH}/FreeRTOS-Plus-TCP)

# Add include paths
#target_include_directories(freertos_plus_tcp
#    INTERFACE
#    # Add user defined include paths
#    "${CMAKE_SOURCE_DIR}/Core/Inc"
##    "${CMAKE_SOURCE_DIR}/Middleware/Third_Party/FreeRTOS-Kernel/include"
##    "${CMAKE_SOURCE_DIR}/Middleware/Third_Party/FreeRTOS-Kernel/portable/GCC/ARM_CM33_NTZ/non_secure"
#    "${CMAKE_SOURCE_DIR}/Drivers/STM32H5xx_HAL_Driver/Inc"
#    "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include"
#    "${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32H5xx/Include"
#)


# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
#    ${CMAKE_SOURCE_DIR}/Core/Src/App.c
    ${CMAKE_SOURCE_DIR}/Core/Src/fatfs.c
    ${CMAKE_SOURCE_DIR}/Core/Src/sd_diskio.c
    ${CMAKE_SOURCE_DIR}/Core/Src/FreeRTOS_CLI.c
    ${CMAKE_SOURCE_DIR}/Core/Src/cli_app.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${THIRD_PARTY_PATH}/FatFs/src
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
#    freertos_plus_tcp
    fatfs
)
