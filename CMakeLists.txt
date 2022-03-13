# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.12)

set(VRS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# In Meta's repo, OOS-only bits are in an an "oss/" sub-folder
# OSS_DIR is a relative path to bridge the repo differences
# so this CMakeLists.txt file works with both layouts.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/oss")
  set(OSS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/oss")
else()
  set(OSS_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()


set(CMAKE_MODULE_PATH "${OSS_DIR}/cmake")

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
add_subdirectory(vrs/utils)
add_subdirectory(vrs/utils/xxhash)
add_subdirectory(vrs/utils/cli)

# Include oss replacement libraries
add_subdirectory(vrs/oss)

# Add the sample apps/code
add_subdirectory(sample_apps)
add_subdirectory(sample_code)
