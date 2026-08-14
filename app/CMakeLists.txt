cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)

add_executable(app_main main.c)

target_sources(app_main
     PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            ${PARENT_DIR}/tusb_src
            ${PARENT_DIR}/file_processing
            ${PARENT_DIR}/hardware_processing
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${PICO_TINYUSB_PATH}/hw/bsp
        FILES
            ${PARENT_DIR}/file_processing/file_processing.h
            ${PICO_TINYUSB_PATH}/hw/bsp/board_api.h)

## target_include_directories(${EXECUTABLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
##target_compile_definitions(app_main PUBLIC CFG_TUH_ENABLED=1)



target_link_libraries(app_main PRIVATE file_processing tinyusb_host )
# target_include_directories(hardware_processing
#     PUBLIC
#         ${CMAKE_CURRENT_SOURCE_DIR}
#         ${PARENT_DIR}/Pico-PIO-USB/src
# )

# set_target_properties(${lib_name} PROPERTIES
#     VS_DEBUGGER_COMMAND $<TARGET_FILE:app_main>
#     VS_DEBUGGER_COMMAND_ARGUMENTS ""
# )
