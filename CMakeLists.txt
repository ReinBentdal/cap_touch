cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(cap_touch)

# root directories from where we might include headers
include_directories(${CMAKE_SOURCE_DIR}/src)

target_sources(app PRIVATE src/main.c)

add_subdirectory(src/bluetooth)
add_subdirectory(src/io)
add_subdirectory(src/utils)
add_subdirectory(src/cap_touch)
