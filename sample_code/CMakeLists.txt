# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

# This library only exists to make sure the sample code actually compiles
file(GLOB SAMPLE_CODE_SRCS *.cpp *.h *.hpp)
add_library(sample_code_lib ${SAMPLE_CODE_SRCS})
target_link_libraries(sample_code_lib
    PUBLIC
        vrs
        vrs_logging
        vrs_os
)
