# project name
project(coolmud)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${coolmud_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

# we need the gnu database library
find_package(GDBM REQUIRED)
include_directories(${GDBM_INCLUDE_DIR})

# and bison as well
find_package(BISON REQUIRED)

add_subdirectory(src/src)
