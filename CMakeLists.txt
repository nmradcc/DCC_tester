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

include(FetchContent)

set(DCC_MANUFACTURER_ID
    145u
    CACHE STRING "Manufacturer ID")

set(DCC_RX_DEQUE_SIZE
    31u
    CACHE STRING "Size of the receiver deque of decoder")
set(DCC_TX_DEQUE_SIZE
    10u
    CACHE STRING "Size of the transmitter deque of command station")

option(DCC_STANDARD_COMPLIANCE "Standard compliance" ON)


# Decoder timing settings
set(DCC_RX_MIN_PREAMBLE_BITS
    10u
    CACHE STRING "Minimum number of preambel bits of decoder")
set(DCC_RX_MIN_BIT_1_TIMING
    52u
    CACHE STRING "Minimum duration of a 1 bit of decoder")
set(DCC_RX_MAX_BIT_1_TIMING
    64u
    CACHE STRING "Maximum duration of a 1 bit of decoder")
set(DCC_RX_MIN_BIT_0_TIMING
    90u
    CACHE STRING "Minimum duration of a 0 bit of decoder")
set(DCC_RX_MAX_BIT_0_TIMING
    119u
    CACHE STRING "Maximum duration of a 0 bit of decoder")


# Command station timing settings
set(DCC_TX_MIN_PREAMBLE_BITS
    17u
    CACHE STRING "Minimum number of preambel bits of command station")
set(DCC_TX_MAX_PREAMBLE_BITS
    30u
    CACHE STRING "Maximum number of preambel bits of command station")

set(DCC_TX_MIN_BIT_1_TIMING
    56u
    CACHE STRING "Minimum duration of a 1 bit of command station")
set(DCC_TX_MAX_BIT_1_TIMING
    60u
    CACHE STRING "Maximum duration of a 1 bit of command station")
set(DCC_TX_MIN_BIT_0_TIMING
    97u
    CACHE STRING "Minimum duration of a 0 bit of command station")
set(DCC_TX_MAX_BIT_0_TIMING
    114u
    CACHE STRING "Maximum duration of a 0 bit of command station")

set(DCC_TX_MIN_BIDI_BIT_TIMING
    57u
    CACHE STRING "Minimum duration of a BiDi bit of command station")
set(DCC_TX_MAX_BIDI_BIT_TIMING
    61u
    CACHE STRING "Maximum duration of a BiDi bit of command station")



FetchContent_Declare(
  DCC
  GIT_REPOSITORY "https://github.com/ZIMO-Elektronik/DCC"
  GIT_TAG v0.44.4)

FetchContent_MakeAvailable(DCC)
target_compile_features(DCC INTERFACE cxx_std_23)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/cmsis_os2.c
    Core/Src/cli_app.c
    Core/Src/command_station.cpp
    Core/Src/decoder.cpp
    Core/Src/SUSI_Master.c
    Core/Src/SUSI_Slave.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/bsp/stm32hXX_Nucleo
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
)
