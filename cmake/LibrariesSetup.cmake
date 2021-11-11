# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# We messed up the capitalization of FindxxHash.cmake, which we can't rename in place.
# For now, let's put that file in a subfolder, until we can move it back here "later"...
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${VRS_SOURCE_DIR}/cmake/more")

find_package(Boost REQUIRED
  COMPONENTS
    filesystem
    chrono
    date_time
    system
    thread)
find_package(Fmt REQUIRED)
find_package(Cereal REQUIRED)
find_package(Lz4 REQUIRED)
find_package(Zstd REQUIRED)
find_package(xxHash REQUIRED)

# Setup unit test infra, but only if unit tests are enabled
if (UNIT_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
endif()
