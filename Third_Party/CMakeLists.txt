cmake_minimum_required(VERSION 3.22)
project(ThirdPartyLibs C)

# Path to LwIP source
set(LWIP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/LwIP)

# Include source file lists
include(${LWIP_DIR}/src/Filelists.cmake)

# Create static library
set(LWIP_BUILD_DOC OFF CACHE BOOL "Disable lwIP documentation build")
add_library(lwip STATIC
    ${lwipnoapps_SRCS}
    ${lwipallapps_SRCS}
#    ${LWIP_DIR}/contrib/ports/freertos/sys_arch.c
    ${LWIP_DIR}/system/OS/sys_arch.c
)

target_include_directories(lwip PUBLIC
    ${LWIP_DIR}/src/include
    ${LWIP_DIR}/src/include/ipv4
    ${LWIP_DIR}/src/include/ipv6
    ${LWIP_DIR}/port
    ${LWIP_DIR}/system
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS/RTOS2/Include
    ../LWIP/Target

)

target_compile_definitions(lwip PUBLIC
    LWIP_TCP=1
    LWIP_UDP=1
    LWIP_DNS=1
    NO_SYS=0  # Using FreeRTOS
)

target_compile_options(lwip PRIVATE -Wall -Wextra)
