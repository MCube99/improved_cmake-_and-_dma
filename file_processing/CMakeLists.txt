cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)

add_library(file_processing STATIC
         file_processing.c
        )
       

target_sources(file_processing 
    PUBLIC
        FILE_SET HEADERS 
        BASE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}
        FILES 
            ${CMAKE_CURRENT_SOURCE_DIR}/file_processing.h 
            )        


target_link_libraries(file_processing PRIVATE hardware_processing pico_stdlib fatfs_lib)


