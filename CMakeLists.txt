cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

include_directories(
        ${CMAKE_SOURCE_DIR}/main
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")

project(magic-lamp-espidf)

