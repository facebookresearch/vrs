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

file(GLOB VRS_OS_SRCS *.cpp *.h *.hpp)
# Remove fb-only source files (if there is still any lying around)
file(GLOB VRS_OS_FB_SRCS *_fb.cpp *_fb.h)
if (VRS_OS_FB_SRCS)
  list(REMOVE_ITEM VRS_OS_SRCS ${VRS_OS_FB_SRCS})
endif()

add_library(vrs_os ${VRS_OS_SRCS})
target_include_directories(vrs_os
  PUBLIC
    $<BUILD_INTERFACE:${VRS_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(vrs_os
  PRIVATE
    vrs_platform
    vrs_logging
    Boost::system
    Boost::filesystem
    Boost::thread
    Boost::chrono
    Boost::date_time
)
target_compile_definitions(vrs_os INTERFACE OSS_BUILD_MODE)

add_library(vrs_platform INTERFACE)
target_include_directories(vrs_platform
  INTERFACE
    $<BUILD_INTERFACE:${VRS_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
# When building with cmake, always enable OSS build mode
target_compile_definitions(vrs_platform INTERFACE OSS_BUILD_MODE)

install(TARGETS vrs_os vrs_platform EXPORT VRSLibTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

if (UNIT_TESTS)
  enable_testing()

  add_executable(test_vrs_os
    test/TimeTest.cpp
    test/EventUnitTest.cpp
    test/UtilsTest.cpp
  )
  target_link_libraries(test_vrs_os
    PUBLIC
      vrs_os
      vrs_oss_testdatadir
      GTest::Main
  )

  gtest_discover_tests(test_vrs_os)
endif()
