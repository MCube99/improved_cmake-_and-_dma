cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)

add_library(file_processing STATIC
         file_processing.c
        )
       

target_sources(file_processing 
    PUBLIC
        FILE_SET HEADERS 
        BASE_DIRS
            ${PARENT_DIR}/hardware_processing
            ${PARENT_DIR}/fatfs
            ${CMAKE_CURRENT_SOURCE_DIR}
        FILES 
            ${PARENT_DIR}/hardware_processing/hardware_processing.h
            ${PARENT_DIR}/fatfs/ff.h
            ${CMAKE_CURRENT_SOURCE_DIR}/file_processing.h 
            ${PARENT_DIR}/hardware_processing/queue.h
            )        


target_link_libraries(file_processing PUBLIC hardware_processing)


