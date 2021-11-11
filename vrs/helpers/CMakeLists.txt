# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

add_library(vrs_helpers_strings INTERFACE)
target_include_directories(vrs_helpers_strings INTERFACE ${VRS_SOURCE_DIR})

file(GLOB VRS_HELPERS_SRCS *.cpp *.h *.hpp)

add_library(vrs_helpers STATIC ${VRS_HELPERS_SRCS})
target_link_libraries(vrs_helpers
    PUBLIC
        vrs_helpers_strings
        vrs_os
        Cereal::Cereal
)
target_include_directories(vrs_helpers PUBLIC ${VRS_SOURCE_DIR})

if (UNIT_TESTS)
  enable_testing()

  add_executable(test_vrs_helpers test/StringsTest.cpp)
  target_link_libraries(test_vrs_helpers
      PUBLIC
        vrs_helpers
        GTest::Main
  )

  gtest_discover_tests(test_vrs_helpers)
endif()
