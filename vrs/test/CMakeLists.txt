# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

enable_testing()

add_executable(testedtool testedtool/testedtool.cpp)

add_library(vrs_test_helpers
  helpers/TestProcess.cpp
  helpers/VRSTestsHelpers.cpp
)
target_link_libraries(vrs_test_helpers
  PUBLIC
    vrs
    vrs_oss_test_helpers
)
target_compile_definitions(vrs_test_helpers PUBLIC GTEST_BUILD)

add_executable(test_vrs
  ContentBlockReaderTest.cpp
  CustomBlockTest.cpp
  DataLayoutFormatTest.cpp
  DataLayoutTest.cpp
  ErrorCodeTest.cpp
  EventLoggerTest.cpp
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
      vrs
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
    vrs
    vrs_oss_testdatadir
    vrs_test_helpers
    GTest::Main
)
target_compile_definitions(test_vrs_file_tests PUBLIC GTEST_BUILD)

gtest_discover_tests(test_vrs)
gtest_discover_tests(test_vrs_file_tests)
