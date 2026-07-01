# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/jiang/esp/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "/Users/jiang/esp/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "/Users/jiang/Downloads/01项目/AirMic Keypad/firmware/esp32_idf_classic_combo/build/bootloader"
  "/Users/jiang/Downloads/01项目/AirMic Keypad/firmware/esp32_idf_classic_combo/build/bootloader-prefix"
  "/Users/jiang/Downloads/01项目/AirMic Keypad/firmware/esp32_idf_classic_combo/build/bootloader-prefix/tmp"
  "/Users/jiang/Downloads/01项目/AirMic Keypad/firmware/esp32_idf_classic_combo/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/jiang/Downloads/01项目/AirMic Keypad/firmware/esp32_idf_classic_combo/build/bootloader-prefix/src"
  "/Users/jiang/Downloads/01项目/AirMic Keypad/firmware/esp32_idf_classic_combo/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/jiang/Downloads/01项目/AirMic Keypad/firmware/esp32_idf_classic_combo/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/jiang/Downloads/01项目/AirMic Keypad/firmware/esp32_idf_classic_combo/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
