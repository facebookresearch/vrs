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

file (GLOB VRS_UTILS_CLI_SRCS *.cpp *.h *.hpp)

add_library(vrs_utils_cli ${VRS_UTILS_CLI_SRCS})
target_include_directories(vrs_utils_cli
  PUBLIC
    $<BUILD_INTERFACE:${VRS_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(vrs_utils_cli
  PUBLIC
    vrs_utils
  PRIVATE
    vrs_helpers
    vrs_logging
)
#install(TARGETS vrs_utils_cli EXPORT VRSLibTargets
#  LIBRARY DESTINATION lib
#  ARCHIVE DESTINATION lib
#  RUNTIME DESTINATION bin
#  INCLUDES DESTINATION include
#)
