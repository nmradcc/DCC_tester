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

option(USE_ASIO_RPC "Use asio_net and rpc_core libraries" ON)

if(USE_ASIO_RPC)
  message(STATUS "Using asio_net and rpc_core libraries")
    FetchContent_Declare(
    asio_net
    GIT_REPOSITORY https://github.com/shuai132/asio_net.git
    GIT_TAG        main  # or specify a commit/tag
    )

    FetchContent_MakeAvailable(asio_net)
else()
  message(STATUS "Using rpc_core libraries")
    FetchContent_Declare(
    rpc_core
    GIT_REPOSITORY https://github.com/shuai132/rpc_core.git
    GIT_TAG        master  # or specify a commit/tag
    )

    FetchContent_MakeAvailable(rpc_core)
endif()


FetchContent_Declare(
  DCC
  GIT_REPOSITORY "https://github.com/ZIMO-Elektronik/DCC"
  GIT_TAG v0.44.4)

FetchContent_MakeAvailable(DCC)

## NMRA Public Domain & Do-it-yourself manufacturer ID for experimental use
target_compile_options(DCC INTERFACE -DDCC_MANUFACTURER_ID=13u)
#target_compile_options(DCC INTERFACE -DDCC_TX_DEQUE_SIZE=10u)

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
    Core/Src/rpc_server.cpp
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
    RPC_CORE_LOG_DISABLE_ALL=1
)


if(USE_ASIO_RPC)
# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    asio_net
    DCC::DCC
)
else()
# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    rpc_core
    DCC::DCC
)
endif()