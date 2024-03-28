# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET "esp32")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP_Test)
littlefs_create_partition_image(littlefs ${CMAKE_CURRENT_SOURCE_DIR}/littlefs_root FLASH_IN_PROJECT)
