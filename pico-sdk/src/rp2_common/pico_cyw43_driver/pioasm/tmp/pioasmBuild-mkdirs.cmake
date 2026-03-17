# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/mmadha/.pico-sdk/sdk/2.2.0/tools/pioasm")
  file(MAKE_DIRECTORY "C:/Users/mmadha/.pico-sdk/sdk/2.2.0/tools/pioasm")
endif()
file(MAKE_DIRECTORY
  "C:/Users/mmadha/OneDrive - Transformers and Rectifiers Ltd/Desktop/bare_api/msc_file_explorer/pioasm"
  "C:/Users/mmadha/OneDrive - Transformers and Rectifiers Ltd/Desktop/bare_api/msc_file_explorer/pioasm-install"
  "C:/Users/mmadha/OneDrive - Transformers and Rectifiers Ltd/Desktop/bare_api/msc_file_explorer/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "C:/Users/mmadha/OneDrive - Transformers and Rectifiers Ltd/Desktop/bare_api/msc_file_explorer/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "C:/Users/mmadha/OneDrive - Transformers and Rectifiers Ltd/Desktop/bare_api/msc_file_explorer/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "C:/Users/mmadha/OneDrive - Transformers and Rectifiers Ltd/Desktop/bare_api/msc_file_explorer/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs Debug;Release;MinSizeRel;RelWithDebInfo)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/mmadha/OneDrive - Transformers and Rectifiers Ltd/Desktop/bare_api/msc_file_explorer/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/mmadha/OneDrive - Transformers and Rectifiers Ltd/Desktop/bare_api/msc_file_explorer/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
