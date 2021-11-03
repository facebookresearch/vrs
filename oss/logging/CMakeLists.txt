# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

file (GLOB VRS_LOGGING_SRCS *.cpp *.h *.hpp)

add_library(vrs_logging STATIC ${VRS_LOGGING_SRCS})
target_include_directories(vrs_logging PUBLIC ${VRS_SOURCE_DIR}/oss)
target_link_libraries(vrs_logging
    PUBLIC
        vrs_platform
        Fmt::Fmt
)
