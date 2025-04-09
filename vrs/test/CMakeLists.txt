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

enable_testing()

add_executable(testedtool testedtool/testedtool.cpp)

add_library(vrs_test_helpers
  helpers/VRSTestsHelpers.cpp
)
if (NOT CMAKE_HOST_WIN32)
  target_sources(vrs_test_helpers PUBLIC helpers/TestProcess.cpp)
endif()
target_link_libraries(vrs_test_helpers
  PUBLIC
    vrslib
    vrs_oss_test_helpers
)
target_compile_definitions(vrs_test_helpers PUBLIC GTEST_BUILD)

add_executable(test_vrslib
  AudioContentBlockReaderTest.cpp
  ContentBlockReaderTest.cpp
  CustomBlockTest.cpp
  DataLayoutFormatTest.cpp
  DataLayoutTest.cpp
  ErrorCodeTest.cpp
  TelemetryLoggerTest.cpp
  FileHandlerFactoryTest.cpp
  FileHandlerTest.cpp
  FrameCompressionTest.cpp
  GetRecordTest.cpp
  MultiRecordFileReaderTest.cpp
  NestedDataLayoutTest.cpp
  RecordFormatFileTest.cpp
  RecordFormatTest.cpp
  RecordManagerTest.cpp
  RecordTest.cpp
)
if (BUILD_WITH_OPUS)
  target_compile_definitions(test_vrslib PRIVATE OPUS_IS_AVAILABLE)
endif()

if (NOT CMAKE_HOST_WIN32)
  target_sources(test_vrslib PUBLIC TestProcessTest.cpp)
endif()
target_link_libraries(test_vrslib
  PUBLIC
    vrslib
    vrs_utils
    vrs_oss_testdatadir
    vrs_test_helpers
    GTest::Main
)
add_dependencies(test_vrslib testedtool)
target_compile_definitions(test_vrslib PUBLIC GTEST_BUILD)
target_link_options(test_vrslib PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/FORCE:MULTIPLE>)

add_executable(test_vrslib_file_tests
  file_tests/ChunkedFileTest.cpp
  file_tests/DeviceSimulatorTest.cpp
  file_tests/FileCacheTest.cpp
  file_tests/RecordableTest.cpp
  file_tests/SimpleFileHandlerTest.cpp
)
target_link_libraries(test_vrslib_file_tests
  PUBLIC
    vrslib
    vrs_oss_testdatadir
    vrs_test_helpers
    GTest::Main
)
target_compile_definitions(test_vrslib_file_tests PUBLIC GTEST_BUILD)
target_link_options(test_vrslib_file_tests PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/FORCE:MULTIPLE>)

gtest_discover_tests(test_vrslib)
gtest_discover_tests(test_vrslib_file_tests)
