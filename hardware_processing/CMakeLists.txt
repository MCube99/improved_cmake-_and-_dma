cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)

set(hardware_processing_Sources ${CMAKE_CURRENT_SOURCE_DIR}/hardware_processing.c ${CMAKE_CURRENT_SOURCE_DIR}/queue.c)
set(hardware_processing_Headers ${CMAKE_CURRENT_SOURCE_DIR}/hardware_processing.h ${CMAKE_CURRENT_SOURCE_DIR}/queue.h)

add_library(hardware_processing STATIC ${hardware_processing_Sources})
pico_generate_pio_header(hardware_processing ${CMAKE_CURRENT_LIST_DIR}/clocked_input.pio)
pico_generate_pio_header(hardware_processing ${CMAKE_CURRENT_LIST_DIR}/keyboard_input.pio)

target_include_directories(hardware_processing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PARENT_DIR}/file_processing
)


target_link_libraries(hardware_processing
    PRIVATE hardware_spi hardware_sync
    PUBLIC hardware_pio hardware_irq hardware_gpio hardware_dma fatfs_lib
)
 

# add_library(string_processing spi_processing.c hardware_processing.h)
# target_link_directories(string_processing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# target_link_libraries(string_processing PUBLIC )