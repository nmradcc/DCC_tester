add_library(fatfs_port STATIC)
target_sources(fatfs_port
    PRIVATE
    diskio.c
    ff.c
    ff_gen_drv.c
    ffsystem_cmsis_os.c
    ffunicode.c
    )

    # Configure your custom include path
target_include_directories(fatfs_port
    PUBLIC
        "../../../Drivers/STM32H5xx_HAL_Driver/Inc"
        "../../../Drivers/CMSIS/Include"
        "../../../Drivers/CMSIS/Device/ST/STM32H5xx/Include")


target_link_libraries(fatfs_port
    PRIVATE
        freertos_kernel)
