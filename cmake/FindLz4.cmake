# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.
#
# - Find Lz4
# Find the Lz4 compression library and includes
#
# Lz4_INCLUDE_DIRS - where to find Lz4.h, etc.
# Lz4_LIBRARIES - List of libraries when using Lz4.
# Lz4_FOUND - True if Lz4 found.

find_path(Lz4_INCLUDE_DIRS NAMES lz4.h)
find_library(Lz4_LIBRARIES NAMES lz4)
mark_as_advanced(Lz4_LIBRARIES Lz4_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lz4 DEFAULT_MSG Lz4_LIBRARIES Lz4_INCLUDE_DIRS)

if (Lz4_FOUND AND NOT (TARGET Lz4::Lz4))
  add_library(Lz4::Lz4 UNKNOWN IMPORTED)
  set_target_properties(Lz4::Lz4
    PROPERTIES
      IMPORTED_LOCATION ${Lz4_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${Lz4_INCLUDE_DIRS})
endif()
