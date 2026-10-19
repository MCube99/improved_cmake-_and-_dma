cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)

set(FATFS_LIB_SOURCES 
 	${CMAKE_CURRENT_SOURCE_DIR}/ff.c 
 	${CMAKE_CURRENT_SOURCE_DIR}/ffsystem.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/ffunicode.c
    )

set(FATFS_LIB_HEADERS_FILES
       ${CMAKE_CURRENT_SOURCE_DIR}/diskio.h
       ${CMAKE_CURRENT_SOURCE_DIR}/ff.h
       ${CMAKE_CURRENT_SOURCE_DIR}/ffconf.h
   )

add_library(fatfs_lib ${FATFS_LIB_SOURCES})


target_sources(fatfs_lib
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
       ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
       ${FATFS_LIB_HEADERS_FILES}
        )

target_link_libraries(fatfs_lib PUBLIC tinyusb_host tinyusb_board pico_stdlib PRIVATE common_headers)