# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

file (GLOB VRS_SRCS *.cpp *.h *.hpp)

add_library(vrs STATIC ${VRS_SRCS})
target_include_directories(vrs PUBLIC ${VRS_SOURCE_DIR})
target_link_libraries(vrs
    PUBLIC
        vrs_helpers
        vrs_logging
        vrs_os
        vrs_xxhash
        Lz4::Lz4
        Zstd::Zstd
)

if (UNIT_TESTS)
  enable_testing()

  add_executable(testedtool test/testedtool/testedtool.cpp)

  add_executable(vrs_test
    test/ContentBlockReaderTest.cpp
    test/CustomBlockTest.cpp
    #    test/DataLayoutFormatTest.cpp
    test/DataLayoutTest.cpp
    test/ErrorCodeTest.cpp
    test/EventLoggerTest.cpp
    #test/FileHandlerFactoryTest.cpp
    test/FileHandlerJsonTest.cpp
    test/FrameCompressionTest.cpp
    #test/GetRecordTest.cpp
    test/MultiRecordFileReaderTest.cpp
    test/NestedDataLayoutTest.cpp
    #test/RecordFormatFileTest.cpp
    test/RecordFormatTest.cpp
    test/RecordManagerTest.cpp
    test/RecordTest.cpp
    #test/TestProcessTest.cpp
  )
  target_link_libraries(vrs_test
      PUBLIC
        vrs
        GTest::Main
  )
  add_dependencies(vrs_test testedtool)
  target_compile_definitions(vrs_test PUBLIC GTEST_BUILD)

#  add_executable(vrs_file_tests
#    #test/file_tests/ChunkedFileTest.cpp
#    #test/file_tests/DawnSimulatorTest.cpp
#    #test/file_tests/FileCacheTest.cpp
#    #test/file_tests/LegacyFormatTest.cpp
#    #test/file_tests/RecordableTest.cpp
#    #test/file_tests/SimpleFileHandlerTest.cpp
#  )
#  target_link_libraries(vrs_file_tests
#      PUBLIC
#        vrs
#        GTest::Main
#  )
#  target_compile_definitions(vrs_file_tests PUBLIC GTEST_BUILD)

#  add_executable(vrs_test_helpers
#    #test/helpers/TestProcess.cpp
#    #test/helpers/VRSTestsHelpers.cpp
#  )
#  target_link_libraries(vrs_test_helpers
#      PUBLIC
#        vrs
#        GTest::Main
#  )
#  target_compile_definitions(vrs_test_helpers PUBLIC GTEST_BUILD)

  gtest_discover_tests(vrs_test)
endif()
