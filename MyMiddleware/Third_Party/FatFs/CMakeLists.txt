
set(FatFs_Include_Dirs
    ${CMAKE_SOURCE_DIR}/FATFS/Target
    ${CMAKE_SOURCE_DIR}/MyMiddleware/Third_Party/FatFs/src
)

set(fatfs_Source
    ${CMAKE_SOURCE_DIR}/MyMiddleware/Third_Party/FatFs/src/diskio.c
    ${CMAKE_SOURCE_DIR}/MyMiddleware/Third_Party/FatFs/src/ff.c
    ${CMAKE_SOURCE_DIR}/MyMiddleware/Third_Party/FatFs/src/ff_gen_drv.c
    ${CMAKE_SOURCE_DIR}/MyMiddleware/Third_Party/FatFs/src/option/syscall.c
)

add_library(fatfs STATIC 
    ${fatfs_Source}
)

target_include_directories(fatfs PUBLIC ${FatFs_Include_Dirs})

