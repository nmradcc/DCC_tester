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
include("gcc-arm-none-eabi.cmake")
#include("starm-clang.cmake")

if(DEFINED STARM_TOOLCHAIN_CONFIG)
    message(STATUS "Using ST toolchain config: ${STARM_TOOLCHAIN_CONFIG}")
endif()
    
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

FetchContent_Declare(
rpc_core
GIT_REPOSITORY https://github.com/shuai132/rpc_core.git
GIT_TAG        master  # or specify a commit/tag
)

FetchContent_MakeAvailable(rpc_core)

FetchContent_Declare(
    openmrn
    GIT_REPOSITORY https://github.com/dvescovi1/openmrn.git
    GIT_TAG header-only  # or specific commit/tag
#    SOURCE_DIR     ${CMAKE_PROJECT_NAME}/openmrn  # Optional: control location
#    SOURCE_DIR     ${CMAKE_BINARY_DIR}/_deps/openmrn-src  # Optional: control location
)

FetchContent_MakeAvailable(openmrn)

FetchContent_Declare(
  DCC
  GIT_REPOSITORY "https://github.com/nmradcc/DCC"
  GIT_TAG DCCTester)

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
#    newlib_lock_glue.c
#    picolibc_threadx_lock.c
    Core/Src/cli_app.c
    Core/Src/rpc_server.cpp
    Core/Src/command_station.cpp
    Core/Src/decoder.cpp
    Core/Src/parameter_manager.c
    Core/Src/analog_manager.c
    Core/Src/SUSI_Master.c
    Core/Src/SUSI_Slave.c
    Core/Src/OpenMRN_client.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${openmrn_SOURCE_DIR}/src  # OpenMRN header-only include
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    RPC_CORE_LOG_DISABLE_ALL=1
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)


# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    rpc_core
    OpenMRN::openmrn
    DCC::DCC
)

