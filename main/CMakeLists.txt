# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources
        ${CMAKE_SOURCE_DIR}/main/*.*
        ${CMAKE_SOURCE_DIR}/main/ir/*.*
        ${CMAKE_SOURCE_DIR}/main/led/*.*
)

idf_component_register(SRCS ${app_sources})

spiffs_create_partition_image(storage ../data)