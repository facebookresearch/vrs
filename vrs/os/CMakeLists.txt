# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

file (GLOB VRS_OS_SRCS *.cpp *.h *.hpp)
list (REMOVE_ITEM VRS_OS_SRCS "Platform.h")

add_library(vrs_os STATIC ${VRS_OS_SRCS})
target_include_directories(vrs_os PUBLIC ${VRS_SOURCE_DIR})
target_link_libraries(vrs_os
    PUBLIC
        vrs_logging
        Boost::system
        Boost::filesystem
        Boost::thread
        Boost::chrono
        Boost::date_time
)

add_library(vrs_platform INTERFACE)
target_include_directories(vrs_platform INTERFACE ${VRS_SOURCE_DIR})
