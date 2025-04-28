# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# - Find FFmpeg
# Find the FFmpeg library

# Specify the installation path for FFmpeg
set(FFmpeg_ROOT "$ENV{HOME}/vrs_third_party_libs/ffmpeg")

# Set the include directories
set(FFmpeg_INCLUDE_DIRS
    "${FFmpeg_ROOT}/include"
)

# Link against ZLIB
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    message(STATUS "Zlib found: ${ZLIB_LIBRARIES}")
else()
    message(FATAL_ERROR "Zlib not found")
endif()

# Create imported targets for each static library
add_library(ffmpeg_avcodec STATIC IMPORTED)
set_target_properties(ffmpeg_avcodec PROPERTIES
    IMPORTED_LOCATION "${FFmpeg_ROOT}/lib/libavcodec.a"
)
add_library(ffmpeg_avformat STATIC IMPORTED)
set_target_properties(ffmpeg_avformat  PROPERTIES
    IMPORTED_LOCATION "${FFmpeg_ROOT}/lib/libavformat.a"
)
add_library(ffmpeg_avutil STATIC IMPORTED)
set_target_properties(ffmpeg_avutil PROPERTIES
    IMPORTED_LOCATION "${FFmpeg_ROOT}/lib/libavutil.a"
)
add_library(ffmpeg_avdevice STATIC IMPORTED)
set_target_properties(ffmpeg_avdevice  PROPERTIES
    IMPORTED_LOCATION "${FFmpeg_ROOT}/lib/libavdevice.a"
)
add_library(ffmpeg_avfilter STATIC IMPORTED)
set_target_properties(ffmpeg_avfilter  PROPERTIES
    IMPORTED_LOCATION "${FFmpeg_ROOT}/lib/libavfilter.a"
)

# Create an imported target for the FFmpeg library
add_library(ffmpeg_decoding INTERFACE)
target_link_libraries(ffmpeg_decoding INTERFACE
    ffmpeg_avformat
    ffmpeg_avcodec
    ffmpeg_avutil
    ffmpeg_avdevice
    ffmpeg_avfilter
    ZLIB::ZLIB
)
# Set the include directories for the interface target
target_include_directories(ffmpeg_decoding INTERFACE "${FFmpeg_INCLUDE_DIRS}")

message(STATUS "Found FFmpeg lib, with include dirs: ${FFmpeg_INCLUDE_DIRS}")
