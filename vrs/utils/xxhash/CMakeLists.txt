# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

file (GLOB VRS_XXHASH_SRCS *.cpp *.h *.hpp)

add_library(vrs_xxhash STATIC ${VRS_XXHASH_SRCS})
target_include_directories(vrs_xxhash PUBLIC ${VRS_SOURCE_DIR})
target_link_libraries(vrs_xxhash
    PUBLIC
        vrs_logging
        xxHash::xxHash
)

if (UNIT_TESTS)
  enable_testing()

  add_executable(test_vrs_xxhash
    test/xxhashTest.cpp
  )
  target_link_libraries(test_vrs_xxhash
      PUBLIC
        vrs_xxhash
        GTest::Main
  )

  gtest_discover_tests(test_vrs_xxhash)
endif()
