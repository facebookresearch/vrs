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

cmake_minimum_required(VERSION 3.16)

# Make it simpler to find Qt.
# If CMAKE_PREFIX_PATH isn't set and the env variable QT_DIR is, use it!
if ("${CMAKE_PREFIX_PATH}" STREQUAL "" AND EXISTS "$ENV{QT_DIR}")
  message(STATUS "Setting CMAKE_PREFIX_PATH from QT_DIR=$ENV{QT_DIR}")
  set(CMAKE_PREFIX_PATH "$ENV{QT_DIR}")
endif()

# When building within Meta's source tree, a number of OSS-only source files live in an
# "oss/" sub-folder next to the main vrs/ folder that contains vrs' main source code.
if ("${VRS_SOURCE_DIR}" STREQUAL "")
  # open source layout
  set(VRS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  set(OSS_DIR "${VRS_SOURCE_DIR}")
endif()

# To customize where external dependencies are stored
if ("${EXTERNAL_DEPENDENCIES_DIR}" STREQUAL "")
  set(EXTERNAL_DEPENDENCIES_DIR "${CMAKE_BINARY_DIR}/external")
endif()
file(MAKE_DIRECTORY "${EXTERNAL_DEPENDENCIES_DIR}")
message(STATUS "External dependencies at ${EXTERNAL_DEPENDENCIES_DIR}")

# Initialize key cmake settings before the project is configured
set(CMAKE_MODULE_PATH "${OSS_DIR}/cmake")
include(${CMAKE_MODULE_PATH}/Platform.cmake)
include(${CMAKE_MODULE_PATH}/Compiler.cmake)
include(${CMAKE_MODULE_PATH}/Options.cmake)

# --- Version numbers for generated libraries ---
file(READ "${OSS_DIR}/version.txt" VRS_DEFAULT_VERSION)
# Strip trailing newline
string(REGEX REPLACE "\n$" "" VRS_DEFAULT_VERSION "${VRS_DEFAULT_VERSION}")
if ("${VRS_DEFAULT_VERSION}x" STREQUAL "x")
  message(FATAL_ERROR "Could not get version number from file 'version.txt'")
endif()

# Declare the project, make cmake chose/test the compiler, etc
project(vrs
  VERSION ${VRS_DEFAULT_VERSION}
  DESCRIPTION "Meta VRS File Format Project"
  LANGUAGES CXX
)
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")

# If available, use ccache
include(${CMAKE_MODULE_PATH}/CCache.cmake)

# Library setup
include(${CMAKE_MODULE_PATH}/LibrariesSetup.cmake)

# Include the libraries
add_subdirectory(vrs)

OPTION(BUILD_TOOLS "Build all of the tools" ON)
OPTION(BUILD_SAMPLES "Build sample code and sample apps" ON)
OPTION(BUILD_WITH_XPRS "Build with XPRS decoding library" OFF)

# Add xprs folder if BUILD_WITH_XPRS is set
if (BUILD_WITH_XPRS)
  add_subdirectory(${VRS_SOURCE_DIR}/xprs)
endif()

if (BUILD_TOOLS)
  # Add the vrs command line utility
  add_subdirectory(${OSS_DIR}/tools/vrs)

  # Add the vrsplayer tool
  add_subdirectory(${OSS_DIR}/tools/vrsplayer)
endif()

if (BUILD_SAMPLES)
  # Add the sample apps/code
  add_subdirectory(sample_apps)
  add_subdirectory(sample_code)
endif()
