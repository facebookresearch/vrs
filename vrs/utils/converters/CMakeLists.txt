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

file (GLOB VRS_UTILS_CONVERTERS_SRCS *.cpp *.h *.hpp)
add_library(vrs_utils_converters STATIC ${VRS_UTILS_CONVERTERS_SRCS})
target_link_libraries(vrs_utils_converters
  PUBLIC
    vrs_logging
    vrs_os
)
target_include_directories(vrs_utils_converters PUBLIC ${VRS_SOURCE_DIR})

if (UNIT_TESTS)
  enable_testing()

  add_executable(test_vrs_utils_converters
    test/Raw10ToGrey10ConverterTest.cpp
  )
  target_link_libraries(test_vrs_utils_converters
    PUBLIC
      vrs_utils_converters
      GTest::Main
  )

  gtest_discover_tests(test_vrs_utils_converters)
endif()
