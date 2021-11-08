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
    vrs_oss_misc
)
target_compile_definitions(vrs_test_helpers PUBLIC GTEST_BUILD)

add_executable(vrs_test
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
target_link_libraries(vrs_test
    PUBLIC
      vrs
      vrs_oss_misc
      vrs_test_helpers
      GTest::Main
)
add_dependencies(vrs_test testedtool)
target_compile_definitions(vrs_test PUBLIC GTEST_BUILD)

add_executable(vrs_file_tests
  file_tests/ChunkedFileTest.cpp
  file_tests/DawnSimulatorTest.cpp
  file_tests/FileCacheTest.cpp
  file_tests/RecordableTest.cpp
  file_tests/SimpleFileHandlerTest.cpp
)
target_link_libraries(vrs_file_tests
  PUBLIC
    vrs
    vrs_oss_misc
    vrs_test_helpers
    GTest::Main
)
target_compile_definitions(vrs_file_tests PUBLIC GTEST_BUILD)

gtest_discover_tests(vrs_test)
gtest_discover_tests(vrs_file_tests)
