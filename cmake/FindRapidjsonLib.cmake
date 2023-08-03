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

# The package version of rapidjson provided by apt on Linux or brew on Mac date from August 2016,
# despite being the last official version (v1.1.0), and don't include important fixes.
# So we get the code from GitHub at a known working state.
find_path(RAPIDJSON_INSTALL NAMES rapidjson/rapidjson.h)
if (RAPIDJSON_INSTALL)
  message(WARNING "Found RapidJson at ${RAPIDJSON_INSTALL} It might be used instead "
    "of VRS' version, and cause builds errors because the code is too old.")
endif()

set(RAPIDJSON_DIR "${EXTERNAL_DEPENDENCIES_DIR}/rapidjson")
set(RAPIDJSON_INCLUDE_DIR "${RAPIDJSON_DIR}/include")

if (NOT EXISTS "${RAPIDJSON_INCLUDE_DIR}")
  execute_process(COMMAND git clone https://github.com/Tencent/rapidjson.git "${RAPIDJSON_DIR}")
endif()
execute_process(COMMAND cd "${RAPIDJSON_DIR}" && git -f checkout a95e013b97ca6523f32da23f5095fcc9dd6067e5)
if (NOT EXISTS "${RAPIDJSON_INCLUDE_DIR}/rapidjson/rapidjson.h")
  message(FATAL_ERROR "Could not setup rapidjson external dependency at ${RAPIDJSON_DIR}")
endif()

add_library(rapidjson::rapidjson INTERFACE IMPORTED)
set_target_properties(
  rapidjson::rapidjson PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${RAPIDJSON_INCLUDE_DIR}"
)
add_dependencies(rapidjson::rapidjson rapidjson)

message(STATUS "Found RapidJson: ${RAPIDJSON_DIR}")
