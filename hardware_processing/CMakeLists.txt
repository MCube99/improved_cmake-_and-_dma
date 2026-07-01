cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)

set(hardware_processing_Sources ${CMAKE_CURRENT_SOURCE_DIR}/hardware_processing.c ${CMAKE_CURRENT_SOURCE_DIR}/queue.c ${CMAKE_CURRENT_SOURCE_DIR}/ring_buf.c)
set(hardware_processing_Headers ${CMAKE_CURRENT_SOURCE_DIR}/hardware_processing.h ${CMAKE_CURRENT_SOURCE_DIR}/queue.h ${CMAKE_CURRENT_SOURCE_DIR}/ring_buf.h)
set(common_processing_Headers   ${CMAKE_CURRENT_SOURCE_DIR}/common_header.h)

add_library(hardware_processing STATIC ${hardware_processing_Sources})
add_library(common_headers INTERFACE )
pico_generate_pio_header(hardware_processing ${CMAKE_CURRENT_LIST_DIR}/clocked_input.pio)
pico_generate_pio_header(hardware_processing ${CMAKE_CURRENT_LIST_DIR}/keyboard_input.pio)


target_sources(hardware_processing PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${hardware_processing_Headers} )

target_sources(common_headers PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES ${common_processing_Headers}   
)

target_link_libraries(common_headers INTERFACE pico_stdlib)

target_link_libraries(hardware_processing
    PRIVATE hardware_sync hardware_irq hardware_gpio hardware_dma fatfs_lib hardware_spi
    PUBLIC common_headers hardware_pio )
    
