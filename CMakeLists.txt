# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

cmake_minimum_required(VERSION 3.12)

set(VRS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_MODULE_PATH "${VRS_SOURCE_DIR}/cmake")

include(${CMAKE_MODULE_PATH}/Platform.cmake)
include(${CMAKE_MODULE_PATH}/Compiler.cmake)
include(${CMAKE_MODULE_PATH}/Options.cmake)

# Declare the project, make cmake chose/test the compiler, etc
project(vrs
  VERSION 1.0
  DESCRIPTION "Meta VRS File Format Project"
  LANGUAGES CXX)
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")

# If available, use ccache
include(${CMAKE_MODULE_PATH}/CCache.cmake)

# Library setup
include(${CMAKE_MODULE_PATH}/LibrariesSetup.cmake)

# Include the libraries
add_subdirectory(vrs)
add_subdirectory(vrs/helpers)
add_subdirectory(vrs/os)
add_subdirectory(vrs/utils/xxhash)

# Include oss replacement libraries
add_subdirectory(oss/logging)
add_subdirectory(oss/misc)

# Add the apps/tools
add_subdirectory(sample_apps)
