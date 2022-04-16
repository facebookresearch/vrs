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

# This library is a minimal replacement for the portability library used at Meta.

add_library(vrs_oss_portability INTERFACE)
set_target_properties(
  vrs_oss_portability PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES ${VRS_SOURCE_DIR}/vrs/oss)
target_link_libraries(vrs_oss_portability
  INTERFACE
    Boost::filesystem)

install(TARGETS vrs_oss_portability EXPORT VRSLibTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)
