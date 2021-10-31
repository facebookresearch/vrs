# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

file (GLOB VRS_SRCS *.cpp *.h *.hpp)

add_library(vrs STATIC ${VRS_SRCS})
target_include_directories(vrs PUBLIC ${VRS_SOURCE_DIR})
target_link_libraries(vrs
    PUBLIC
        vrs_helpers
        vrs_logging
        vrs_os
        vrs_xxhash
        Lz4::Lz4
        Zstd::Zstd
)
