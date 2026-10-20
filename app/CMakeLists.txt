cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)

add_executable(app_main main.c)

target_include_directories(app_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})


target_link_libraries(app_main PRIVATE file_processing hardware_processing tinyusb_host host_storage )
# target_include_directories(hardware_processing
#     PUBLIC
#         ${CMAKE_CURRENT_SOURCE_DIR}
#         ${PARENT_DIR}/Pico-PIO-USB/src
# )

# set_target_properties(${lib_name} PROPERTIES
#     VS_DEBUGGER_COMMAND $<TARGET_FILE:app_main>
#     VS_DEBUGGER_COMMAND_ARGUMENTS ""
# )
