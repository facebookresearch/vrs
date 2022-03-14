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
  helpers/TestProcess.cpp
  helpers/VRSTestsHelpers.cpp
)
target_link_libraries(vrs_test_helpers
  PUBLIC
    vrslib
    vrs_oss_test_helpers
)
target_compile_definitions(vrs_test_helpers PUBLIC GTEST_BUILD)

add_executable(test_vrs
  ContentBlockReaderTest.cpp
  CustomBlockTest.cpp
  DataLayoutFormatTest.cpp
  DataLayoutTest.cpp
  ErrorCodeTest.cpp
  TelemetryLoggerTest.cpp
  FileHandlerFactoryTest.cpp
  FileHandlerJsonTest.cpp
  FrameCompressionTest.cpp
  GetRecordTest.cpp
  MultiRecordFileReaderTest.cpp
  NestedDataLayoutTest.cpp
  RecordFormatFileTest.cpp
  RecordFormatTest.cpp
  RecordManagerTest.cpp
  RecordTest.cpp
  TestProcessTest.cpp
)
target_link_libraries(test_vrs
    PUBLIC
      vrslib
      vrs_oss_testdatadir
      vrs_test_helpers
      GTest::Main
)
add_dependencies(test_vrs testedtool)
target_compile_definitions(test_vrs PUBLIC GTEST_BUILD)

add_executable(test_vrs_file_tests
  file_tests/ChunkedFileTest.cpp
  file_tests/DeviceSimulatorTest.cpp
  file_tests/FileCacheTest.cpp
  file_tests/RecordableTest.cpp
  file_tests/SimpleFileHandlerTest.cpp
)
target_link_libraries(test_vrs_file_tests
  PUBLIC
    vrslib
    vrs_oss_testdatadir
    vrs_test_helpers
    GTest::Main
)
target_compile_definitions(test_vrs_file_tests PUBLIC GTEST_BUILD)

gtest_discover_tests(test_vrs)
gtest_discover_tests(test_vrs_file_tests)
