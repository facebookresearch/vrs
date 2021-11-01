# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

add_library(vrs_helpers_strings INTERFACE)
target_include_directories(vrs_helpers_strings INTERFACE ${VRS_SOURCE_DIR})

file(GLOB VRS_HELPERS_SRCS *.cpp *.h *.hpp)

add_library(vrs_helpers STATIC ${VRS_HELPERS_SRCS})
target_link_libraries(vrs_helpers
    PUBLIC
        vrs_helpers_strings
        vrs_os
        Cereal::Cereal
)
target_include_directories(vrs_helpers PUBLIC ${VRS_SOURCE_DIR})
