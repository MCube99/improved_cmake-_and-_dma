add_library(tusb_host INTERFACE)
target_sources(tusb_host INTERFACE ${CMAKE_CURRENT_LIST_DIR}/tusb_config.h)