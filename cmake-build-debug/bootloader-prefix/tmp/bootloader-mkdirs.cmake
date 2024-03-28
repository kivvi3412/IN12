# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/kivvi/Storage/Software_Library/ESP_Projects/esp-idf/components/bootloader/subproject"
  "/Users/kivvi/Documents/JetBrains_Project/CLion/ESP32/ESP_Test/cmake-build-debug/bootloader"
  "/Users/kivvi/Documents/JetBrains_Project/CLion/ESP32/ESP_Test/cmake-build-debug/bootloader-prefix"
  "/Users/kivvi/Documents/JetBrains_Project/CLion/ESP32/ESP_Test/cmake-build-debug/bootloader-prefix/tmp"
  "/Users/kivvi/Documents/JetBrains_Project/CLion/ESP32/ESP_Test/cmake-build-debug/bootloader-prefix/src/bootloader-stamp"
  "/Users/kivvi/Documents/JetBrains_Project/CLion/ESP32/ESP_Test/cmake-build-debug/bootloader-prefix/src"
  "/Users/kivvi/Documents/JetBrains_Project/CLion/ESP32/ESP_Test/cmake-build-debug/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/kivvi/Documents/JetBrains_Project/CLion/ESP32/ESP_Test/cmake-build-debug/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/kivvi/Documents/JetBrains_Project/CLion/ESP32/ESP_Test/cmake-build-debug/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
