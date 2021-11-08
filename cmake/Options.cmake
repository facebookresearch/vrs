# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

set(OSS_BUILD_MODE ON CACHE BOOL "Force OSS build mode.")
if ("${OSS_BUILD_MODE}")
  message(STATUS "Forcing OSS build mode")
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
    "Type of build: Debug or Release." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

set(UNIT_TESTS ON CACHE BOOL "Disable unit tests.")
