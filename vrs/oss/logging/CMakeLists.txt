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

# This library is a minimal replacement for the logging library used at Meta.
# The logging output probably should be redirected by the users of VRS to their own logging infra.

file (GLOB VRS_LOGGING_SRCS *.cpp *.h *.hpp)

add_library(vrs_logging ${VRS_LOGGING_SRCS})
target_include_directories(vrs_logging
  INTERFACE
  $<BUILD_INTERFACE:${VRS_SOURCE_DIR}/vrs/oss>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(vrs_logging
    PUBLIC
        vrs_platform
        Fmt::Fmt
)

install(TARGETS vrs_logging EXPORT VRSLibTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)
