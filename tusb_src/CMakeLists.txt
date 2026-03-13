
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)


set(SRC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/msc_app.c
    )

set(SRC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/msc_app.h
    ${CMAKE_CURRENT_SOURCE_DIR}/embedded_cli.h)

add_library(host_storage STATIC
    ${SRC_SOURCES}
)

target_sources(host_storage
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
       ${PARENT_DIR}/fatfs
       ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        ${SRC_HEADERS}
        ${PARENT_DIR}/fatfs/ff.h
        ${PARENT_DIR}/fatfs/diskio.h
        )
       


target_link_libraries(host_storage
    PUBLIC
        file_processing
        PRIVATE tinyusb_host tinyusb_board 
)
