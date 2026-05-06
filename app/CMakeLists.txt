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
        FILES
            ${PARENT_DIR}/file_processing/file_processing.h
            ${PARENT_DIR}/hardware_processing/hardware_processing.h
)

## target_include_directories(${EXECUTABLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
##target_compile_definitions(app_main PUBLIC CFG_TUH_ENABLED=1)



target_link_libraries(app_main PRIVATE file_processing ${lib_name} host_storage pico_multicore hardware_clocks)
# target_include_directories(hardware_processing
#     PUBLIC
#         ${CMAKE_CURRENT_SOURCE_DIR}
#         ${PARENT_DIR}/Pico-PIO-USB/src
# )

# set_target_properties(${lib_name} PROPERTIES
#     VS_DEBUGGER_COMMAND $<TARGET_FILE:app_main>
#     VS_DEBUGGER_COMMAND_ARGUMENTS ""
# )


message(STATUS "Linked libs for ${lib_name}: ${LINK_LIBS}")

pico_enable_stdio_uart(app_main 0)
pico_add_extra_outputs(app_main)
