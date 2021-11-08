# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

find_path(xxhash_INCLUDE_DIRS
  NAME xxhash.h
  PATH_SUFFIXES include
  HINTS ${xxHash_ROOT_DIR}/include)

find_library(xxHash_LIBRARIES
  NAMES xxhash xxHash
  PATH_SUFFIXES lib
  HINTS ${xxHash_ROOT_DIR}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(xxHash DEFAULT_MSG xxHash_LIBRARIES xxhash_INCLUDE_DIRS)

mark_as_advanced(xxHash_LIBRARIES xxhash_INCLUDE_DIRS)

if (xxHash_FOUND AND NOT (TARGET xxHash::xxHash))
  add_library(xxHash::xxHash UNKNOWN IMPORTED)
  set_target_properties(xxHash::xxHash
    PROPERTIES
      IMPORTED_LOCATION ${xxHash_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${xxhash_INCLUDE_DIRS})
endif()
