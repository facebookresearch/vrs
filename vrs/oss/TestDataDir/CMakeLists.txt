# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# This library is a minimal replacement for the TestDataDir library used at Facebook.

file (GLOB VRS_OSS_TESTDATADIR *.cpp *.h)

add_library(vrs_oss_testdatadir STATIC ${VRS_OSS_TESTDATADIR})
target_include_directories(vrs_oss_testdatadir PUBLIC ${VRS_SOURCE_DIR}/vrs/oss)
target_link_libraries(vrs_oss_testdatadir
  PUBLIC
    vrs_os)
target_compile_definitions(vrs_oss_testdatadir PRIVATE VRS_SOURCE_DIR="${VRS_SOURCE_DIR}")
