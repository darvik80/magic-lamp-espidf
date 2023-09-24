cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

list(APPEND EXTRA_COMPONENT_DIRS libs/core-esp32)
list(APPEND EXTRA_COMPONENT_DIRS 3pl-libs/led_strip)

project(magic-lamp-espidf)

