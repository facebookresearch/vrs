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

file (GLOB VRS_SRCS *.cpp *.h *.hpp)

add_library(vrs STATIC ${VRS_SRCS})
target_include_directories(vrs PUBLIC ${VRS_SOURCE_DIR})
target_link_libraries(vrs
  PUBLIC
    vrs_helpers
    vrs_logging
    vrs_os
    vrs_utils_xxhash
    Lz4::Lz4
    Zstd::Zstd
)

if (UNIT_TESTS)
  add_subdirectory(test)
endif()
