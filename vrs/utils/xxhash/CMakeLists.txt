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

file (GLOB VRS_XXHASH_SRCS *.cpp *.h *.hpp)

add_library(vrs_utils_xxhash ${VRS_XXHASH_SRCS})
target_include_directories(vrs_utils_xxhash
  PUBLIC
    $<BUILD_INTERFACE:${VRS_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(vrs_utils_xxhash
  PRIVATE
    vrs_logging
    xxHash::xxHash
)
install(TARGETS vrs_utils_xxhash EXPORT VRSLibTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

if (UNIT_TESTS)
  enable_testing()

  add_executable(test_vrs_utils_xxhash
    test/xxhashTest.cpp
  )
  target_link_libraries(test_vrs_utils_xxhash
    PUBLIC
      vrs_utils_xxhash
      GTest::Main
  )

  gtest_discover_tests(test_vrs_utils_xxhash)
endif()
