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

add_library(vrs_helpers_strings INTERFACE)
target_include_directories(vrs_helpers_strings
  INTERFACE
    $<BUILD_INTERFACE:${VRS_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

file(GLOB VRS_HELPERS_SRCS *.cpp *.h *.hpp)

add_library(vrs_helpers ${VRS_HELPERS_SRCS})
target_link_libraries(vrs_helpers
  PRIVATE
    vrs_helpers_strings
  PUBLIC
    vrs_os
    Cereal::Cereal
)
target_include_directories(vrs_helpers
  PUBLIC
    $<BUILD_INTERFACE:${VRS_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

install(TARGETS vrs_helpers vrs_helpers_strings EXPORT VRSLibTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

if (UNIT_TESTS)
  enable_testing()

  add_executable(test_vrs_helpers test/StringsTest.cpp)
  target_link_libraries(test_vrs_helpers
    PUBLIC
      vrs_helpers
      GTest::Main
  )

  gtest_discover_tests(test_vrs_helpers)
endif()
