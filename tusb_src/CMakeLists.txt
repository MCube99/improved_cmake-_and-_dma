
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)


set(SRC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/msc_app.c ${CMAKE_CURRENT_SOURCE_DIR}/hid_app.c ${CMAKE_CURRENT_SOURCE_DIR}/hid_host.c)

set(SRC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/msc_app.h
    ${CMAKE_CURRENT_SOURCE_DIR}/embedded_cli.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hid_host.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hid.h
    )


add_library(host_storage STATIC
    ${SRC_SOURCES}
)

target_sources(host_storage
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
       ${CMAKE_CURRENT_SOURCE_DIR}
      ##  ${PICO_TINYUSB_PATH}/hw/bsp
      ## ${PARENT_DIR}/hardware_processing
    FILES
        ${SRC_HEADERS} ##should include all the hid.h stuff in there
     ##   ${PICO_TINYUSB_PATH}/hw/bsp/board_api.h
       ## ${PARENT_DIR}/hardware_processing/queue.h
        )
       

target_link_libraries(host_storage
        PRIVATE tinyusb_host tinyusb_board hardware_processing fatfs_lib 
)
