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

file(GLOB TOOLS_VRS_SRCS *.cpp *.h)
add_executable(vrs ${TOOLS_VRS_SRCS})
target_include_directories(vrs
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(vrs
  PRIVATE
    vrslib
    vrs_utils
    vrs_utils_cli
    fmt::fmt
    PNG::PNG
    JPEG::JPEG
    vrs_logging
)

# Conditionally link to vrs_utils_xprs if INCLUDE_XPRS is set
  if (BUILD_WITH_XPRS)
    target_link_libraries(vrs PRIVATE vrs_utils_xprs)
  endif()


install(TARGETS vrs EXPORT VRSLibTargets
  RUNTIME DESTINATION bin
)

if (UNIT_TESTS)
  enable_testing()

  add_executable(test_vrscli
    VrsCommand.cpp
    test/VrsProcess.h
    test/VrsCommandTest.cpp
  )
  if (NOT CMAKE_HOST_WIN32)
    target_sources(test_vrscli PUBLIC test/VrsAppTest.cpp)
  endif()
  target_include_directories(test_vrscli
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/..
  )
  target_link_libraries(test_vrscli
    PUBLIC
      vrs_os
      vrs_test_helpers
      vrs_utils
      vrs_utils_cli
      vrs_oss_testdatadir
      GTest::Main
  )
  add_dependencies(test_vrscli vrs)
  target_link_options(test_vrscli PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/FORCE:MULTIPLE>)

  gtest_discover_tests(test_vrscli)
endif()
