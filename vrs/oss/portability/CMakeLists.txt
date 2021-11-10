# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# This library is a minimal replacement for the portability library used at Facebook.

add_library(vrs_oss_portability INTERFACE Filesystem.h)
set_target_properties(
  vrs_oss_portability PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${VRS_SOURCE_DIR}/vrs/oss")
target_link_libraries(vrs_oss_portability
  INTERFACE
    Boost::filesystem)
