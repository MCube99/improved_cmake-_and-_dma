cmake_minimum_required(VERSION 3.23)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
message(STATUS "PICO_TINYUSB_PATH: ${PICO_TINYUSB_PATH}")
set(CMAKE_VERBOSE_MAKEFILE ON)
set(PICO_TOOLCHAIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/toolchains)
message(STATUS "PICO_TOOLCHAIN_DIR: ${PICO_TOOLCHAIN_DIR}")


# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(PICO_USE_PREBUILT_PICOTOOL ON CACHE BOOL "" FORCE)
set(PICOTOOL_FETCH_FROM_GIT OFF CACHE BOOL "" FORCE)
set(picotool_DIR "H:/picotool/install" CACHE PATH "" FORCE)
set(PICO_NO_PICOTOOL 1)
set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_SDK_PATH "C:/pico-sdk" CACHE PATH "Path to the Raspberry Pi Pico SDK" FORCE)
set(PICO_TINYUSB_PATH "${PICO_SDK_PATH}/lib/tinyusb" CACHE PATH "Path to the TinyUSB path" FORCE)
# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(msc_file_explorer C CXX ASM)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_compile_options(
        -Wall                # Enable most warning messages
        -Wextra              # Enable extra warning messages
        -Wno-unused-parameter # Warn about unused parameters in functions
        -Wno-unused-function  # Warn about unused functions
        -Wno-error=free-nonheap-object
        )



# function(check_pico_tinyusb_paths)

#     list(APPEND path_messages "PICO_TINYUSB_PATH: ${PICO_TINYUSB_PATH}")
#     message(STATUS "PICO_TINYUSB_PATH: ${PICO_TINYUSB_PATH}")
#     list(APPEND path_messages "PICO_TOOLCHAIN_DIR: ${PICO_TOOLCHAIN_DIR}")
#     list(APPEND path_messages "PICO_SDK_PATH: ${PICO_SDK_PATH}")

# endfunction()

# function(check_pico_sdk_paths)


# endfunction()


include_directories(tusb_config) ##include in all
## add_subdirectory(tusb_config)
add_subdirectory(fatfs)
add_subdirectory(hardware_processing)
add_subdirectory(file_processing)
add_subdirectory(tusb_src)
add_subdirectory(app)