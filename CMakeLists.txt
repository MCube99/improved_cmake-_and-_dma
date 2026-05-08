# # Generated Cmake Pico project file

# cmake_minimum_required(VERSION 3.23)

# set(CMAKE_C_STANDARD 11)
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS        OFF)


# # Initialise pico_sdk from installed location
# # (note this can come from environment, CMake cache etc)

# # == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
# if(WIN32)
#     set(USERHOME $ENV{USERPROFILE})
# else()
#     set(USERHOME $ENV{HOME})
# endif()
# set(sdkVersion 2.2.0)
# set(toolchainVersion 14_2_Rel1)
# set(picotoolVersion 2.2.0)
# set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)

# set(LIBRARY_NAME LIBRARY)
# set(EXECUTABLE_NAME EXECUTABLE)

# set(PICO_TINYUSB_PATH "$ENV{PICO_TINYUSB_PATH}")
# # Ensure trailing slash
# if(NOT PICO_TINYUSB_PATH MATCHES ".*/$")
#     set(PICO_TINYUSB_PATH "${PICO_TINYUSB_PATH}/")
# endif()

# # Make it propagate
# set(PICO_TINYUSB_PATH "${PICO_TINYUSB_PATH}" CACHE INTERNAL "")


# set(FATFS_LIB_SOURCES 
#  $ENV{PICO_TINYUSB_PATH}/lib/fatfs/source/ff.c 
#  $ENV{PICO_TINYUSB_PATH}/lib/fatfs/source/ffsystem.c 
#  $ENV{PICO_TINYUSB_PATH}/lib/fatfs/source/ffunicode.c)


# message(STATUS "ENV PICO_TINYUSB_PATH = '$ENV{PICO_TINYUSB_PATH}'")
# message(STATUS "FATFS_LIB_HEADERS = '${FATFS_LIB_HEADERS}'")


# set(FATFS_LIB_HEADERS
#     "$ENV{PICO_TINYUSB_PATH}/lib/fatfs/source"
#     CACHE INTERNAL "FatFs include path"
# )

# set(FATFS_LIB_SOURCES
#     "$ENV{PICO_TINYUSB_PATH}/lib/fatfs/source/ff.c" 
#     "$ENV{PICO_TINYUSB_PATH}/lib/fatfs/source/ffsystem.c" 
#     "$ENV{PICO_TINYUSB_PATH}/lib/fatfs/source/ffunicode.c"
#     CACHE INTERNAL "Fatfs source path"    
# )




# if (EXISTS ${picoVscode})
#     include(${picoVscode})
# endif()
# # ====================================================================================
# set(PICO_BOARD pico CACHE STRING "Board type")

# # Pull in Raspberry Pi Pico SDK (must be before project)
# include(pico_sdk_import.cmake)

# project(msc_file_explorer C CXX ASM)

# # Initialise the Raspberry Pi Pico SDK
# pico_sdk_init()

# # Add executable. Default name is the project name, version 0.1

# include_directories(tusb_config) ##include in all  
# add_subdirectory(fatfs_csv)
# add_subdirectory(file_processing)
# add_subdirectory(string_processing)
# add_subdirectory(src)
# add_subdirectory(app)


# Generated Cmake Pico project file

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
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)


# ---- FIXED CACHE VARIABLE HANDLING ----
# set(PICO_TINYUSB_PATH "$ENV{PICO_TINYUSB_PATH}")
# if(NOT PICO_TINYUSB_PATH MATCHES ".*/$")
#     set(PICO_TINYUSB_PATH "${PICO_TINYUSB_PATH}/")
# endif()
# set(PICO_TINYUSB_PATH "${PICO_TINYUSB_PATH}" CACHE INTERNAL "TinyUSB root path")

# # Use cached variable for all further paths
# set(FATFS_LIB_HEADERS
#     "${PICO_TINYUSB_PATH}lib/fatfs/source"
#     CACHE INTERNAL "FatFs include path"
# )

# set(FATFS_LIB_SOURCES
#     "${PICO_TINYUSB_PATH}lib/fatfs/source/ff.c"
#     "${PICO_TINYUSB_PATH}lib/fatfs/source/ffsystem.c"
#     "${PICO_TINYUSB_PATH}lib/fatfs/source/ffunicode.c"
#     CACHE INTERNAL "FatFs source files"
# )
# ---------------------------------------


if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_SDK_PATH "H:/pico_sdk" CACHE PATH "Path to the Raspberry Pi Pico SDK" FORCE)


# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(msc_file_explorer C CXX ASM)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()


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


