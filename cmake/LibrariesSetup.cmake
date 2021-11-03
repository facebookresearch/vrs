# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# Boost
find_package(Boost REQUIRED
COMPONENTS
    filesystem
    chrono
    date_time
    system
    thread
)

# Fmt
find_package(Fmt REQUIRED)

# Lz4
find_package(Lz4 REQUIRED)

# Zstd
find_package(Zstd REQUIRED)

# Install Cereal
include(${CMAKE_MODULE_PATH}/BuildCereal.cmake)

# Google tests, if enabled
if (UNIT_TESTS)
  include(CTest)
  find_package(GTest REQUIRED)
endif()
