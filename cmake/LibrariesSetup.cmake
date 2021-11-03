# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

find_package(Boost REQUIRED
  COMPONENTS
    filesystem
    chrono
    date_time
    system
    thread)
find_package(Fmt REQUIRED)
find_package(Lz4 REQUIRED)
find_package(Zstd REQUIRED)
find_package(Cereal REQUIRED)

# Setup unit test infra, but only if unit tests are enabled
if (UNIT_TESTS)
  include(CTest)
  find_package(GTest REQUIRED)
endif()
