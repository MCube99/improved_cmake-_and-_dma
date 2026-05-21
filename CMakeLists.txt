cmake_minimum_required(VERSION 3.23)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(PICO_TINYUSB_PATH "$ENV{PICO_TINYUSB_PATH}")
message(STATUS "PICO_TINYUSB_PATH: ${PICO_TINYUSB_PATH}")
set(ENV{PICO_SDK_PATH} "H:/pico_sdk")
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


# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_SDK_PATH "H:/pico_sdk" CACHE PATH "Path to the Raspberry Pi Pico SDK" FORCE)


# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(msc_file_explorer C CXX ASM)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_compile_options(
        -Wall                # Enable most warning messages
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
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
add_subdirectory(fatfs)
add_subdirectory(hardware_processing)
add_subdirectory(file_processing)
add_subdirectory(tusb_src)
add_subdirectory(app)