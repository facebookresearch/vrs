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

if(TARGET rapidjson::rapidjson)
  return()
endif()

# Package managers can opt into a maintained RapidJSON package. Keep VRS'
# pinned source fallback as the default because the official v1.1.0 release is
# too old for VRS.
if(VRS_USE_SYSTEM_RAPIDJSON OR VRS_CONFIG_USE_SYSTEM_RAPIDJSON)
  unset(_RAPIDJSON_TARGET)
  find_package(RapidJSON CONFIG QUIET)

  if(TARGET RapidJSON::rapidjson)
    set(_RAPIDJSON_TARGET RapidJSON::rapidjson)
  elseif(TARGET RapidJSON)
    set(_RAPIDJSON_TARGET RapidJSON)
  elseif(TARGET rapidjson)
    set(_RAPIDJSON_TARGET rapidjson)
  else()
    find_path(RAPIDJSON_INCLUDE_DIR NAMES rapidjson/rapidjson.h)
    if(NOT RAPIDJSON_INCLUDE_DIR)
      message(FATAL_ERROR "VRS_USE_SYSTEM_RAPIDJSON is enabled, but RapidJSON was not found")
    endif()
  endif()

  add_library(rapidjson::rapidjson INTERFACE IMPORTED)
  if(_RAPIDJSON_TARGET)
    set_target_properties(rapidjson::rapidjson PROPERTIES
      INTERFACE_LINK_LIBRARIES "${_RAPIDJSON_TARGET}"
    )
  else()
    set_target_properties(rapidjson::rapidjson PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${RAPIDJSON_INCLUDE_DIR}"
    )
  endif()

  unset(_RAPIDJSON_TARGET)
  return()
endif()

# The package version of rapidjson provided by apt on Linux or brew on Mac dates from August 2016,
# despite being the last official version (v1.1.0), and does not include important fixes.
# So we get the code from GitHub at a known working state.

# Try to find rapidjson in fbsource third-party (for fbsource builds)
# Note: We intentionally do NOT search system paths here, because the system
# rapidjson (v1.1.0 from 2016) is too old and causes build errors.
# OSS builds will fall back to git clone below.
find_path(RAPIDJSON_INCLUDE_DIR
  NAMES rapidjson/rapidjson.h
  HINTS
    # fbsource third-party location
    "${VRS_SOURCE_DIR}/../../../third-party/rapidjson/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../third-party/rapidjson/include"
  NO_DEFAULT_PATH
  NO_CMAKE_ENVIRONMENT_PATH
  NO_CMAKE_SYSTEM_PATH
)

if (RAPIDJSON_INCLUDE_DIR)
  message(STATUS "Found RapidJson at ${RAPIDJSON_INCLUDE_DIR}")
  set(RAPIDJSON_FOUND TRUE)
else()
  # Fall back to downloading from GitHub (OSS behavior)
  if ("${EXTERNAL_DEPENDENCIES_DIR}" STREQUAL "")
    set(EXTERNAL_DEPENDENCIES_DIR "${CMAKE_BINARY_DIR}/external")
  endif()
  set(RAPIDJSON_DIR "${EXTERNAL_DEPENDENCIES_DIR}/rapidjson")
  set(RAPIDJSON_INCLUDE_DIR "${RAPIDJSON_DIR}/include")
  set(RAPIDJSON_INCLUDE_FILE "${RAPIDJSON_INCLUDE_DIR}/rapidjson/rapidjson.h")

  if (NOT EXISTS "${RAPIDJSON_INCLUDE_FILE}")
    execute_process(
      COMMAND git clone https://github.com/Tencent/rapidjson.git "${RAPIDJSON_DIR}"
      RESULT_VARIABLE GIT_CLONE_RESULT
      ERROR_QUIET
    )
  endif()
  if (EXISTS "${RAPIDJSON_DIR}")
    execute_process(
      COMMAND git -C "${RAPIDJSON_DIR}" checkout -f a95e013b97ca6523f32da23f5095fcc9dd6067e5
      RESULT_VARIABLE GIT_CHECKOUT_RESULT
      ERROR_QUIET
    )
  endif()
  if (NOT EXISTS "${RAPIDJSON_INCLUDE_FILE}")
    message(FATAL_ERROR "Could not setup rapidjson external dependency at ${RAPIDJSON_DIR}")
  endif()
  message(STATUS "Found RapidJson: ${RAPIDJSON_DIR}")
endif()

add_library(rapidjson::rapidjson INTERFACE IMPORTED)
set_target_properties(
  rapidjson::rapidjson PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${RAPIDJSON_INCLUDE_DIR}"
)
