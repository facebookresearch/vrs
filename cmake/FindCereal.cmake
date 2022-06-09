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

find_package(cereal QUIET CONFIG)

if (NOT TARGET cereal::cereal)
  find_path(CEREAL_INCLUDE_DIR cereal/cereal.hpp)
  mark_as_advanced(CEREAL_INCLUDE_DIR)

  include(FindPackageHandleStandardArgs)
  set(FPHSA_NAME_MISMATCHED 1) # Suppress warnings, see https://cmake.org/cmake/help/v3.17/module/FindPackageHandleStandardArgs.html
  find_package_handle_standard_args(
    cereal
    DEFAULT_MSG
    CEREAL_INCLUDE_DIR
  )
  unset(FPHSA_NAME_MISMATCHED)
  add_library(cereal::cereal INTERFACE IMPORTED)
  set_target_properties(
    cereal::cereal PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CEREAL_INCLUDE_DIR}"
  )

endif()

target_compile_definitions(cereal::cereal
  INTERFACE
    CEREAL_THREAD_SAFE=1
    CEREAL_RAPIDJSON_NAMESPACE=fb_rapidjson
    CEREAL_RAPIDJSON_PARSE_DEFAULT_FLAGS=kParseFullPrecisionFlag|kParseNanAndInfFlag
    CEREAL_RAPIDJSON_WRITE_DEFAULT_FLAGS=kWriteNoFlags
)
