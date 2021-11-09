# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# This module is a one-stop assembly of "small" dependencies required only to run unit-tests
# It probably can be reduced in scope before vrs is fully open sourced.

file (GLOB VRS_OSS_MISC */*.cpp */*.h)

add_library(vrs_oss_misc STATIC ${VRS_OSS_MISC})
target_include_directories(vrs_oss_misc PUBLIC ${VRS_SOURCE_DIR}/oss/misc)
target_link_libraries(vrs_oss_misc
  PUBLIC
    vrs_platform
    Boost::filesystem
    Fmt::Fmt)
target_compile_definitions(vrs_oss_misc PRIVATE VRS_SOURCE_DIR="${VRS_SOURCE_DIR}")
