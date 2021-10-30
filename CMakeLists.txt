# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

cmake_minimum_required(VERSION 3.9.2)

project(vrs CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})


# Library setup
include(${CMAKE_MODULE_PATH}/LibrariesSetup.cmake)

set(VRS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(vrs)
add_subdirectory(vrs/helpers)
add_subdirectory(vrs/os)
add_subdirectory(vrs/utils/logging)
add_subdirectory(vrs/utils/xxhash)
