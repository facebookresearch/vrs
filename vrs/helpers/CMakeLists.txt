# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

file (GLOB VRS_HELPERS_SRCS *.cpp *.h *.hpp)
list (REMOVE_ITEM VRS_OS_SRCS "Strings.cpp" "Strings.h" "EnumStringConverter.h")
set (VRS_HELPERS_STRINGS_SRCS "Strings.cpp" "Strings.h" "EnumStringConverter.h")

add_library(vrs_helpers_strings STATIC ${VRS_HELPERS_STRINGS_SRCS})
target_link_libraries(vrs_helpers_strings
    PUBLIC
        vrs_platform
        Fmt::Fmt
)
target_include_directories(vrs_helpers_strings PUBLIC ${VRS_SOURCE_DIR})

add_library(vrs_helpers STATIC ${VRS_HELPERS_SRCS})
target_link_libraries(vrs_helpers
    PUBLIC
        vrs_helpers_strings
        vrs_os
        Cereal::Cereal
)
target_include_directories(vrs_helpers PUBLIC ${VRS_SOURCE_DIR})
