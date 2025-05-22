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

# Allow user override of root install prefix
if(NOT FFmpeg_ROOT)
  set(FFmpeg_ROOT "$ENV{HOME}/vrs_third_party_libs/ffmpeg")
endif()

set(_ffmpeg_search_paths
  ${FFmpeg_ROOT}
)

# Headers
find_path(FFMPEG_INCLUDE_DIR
  NAMES libavcodec/avcodec.h
  HINTS ${_ffmpeg_search_paths}
  PATH_SUFFIXES include
)

# Shared libs
find_library(AVCODEC_SO avcodec
  HINTS ${_ffmpeg_search_paths}
  PATH_SUFFIXES lib lib64
)
find_library(AVFORMAT_SO avformat
  HINTS ${_ffmpeg_search_paths}
  PATH_SUFFIXES lib lib64
)
find_library(AVUTIL_SO avutil
  HINTS ${_ffmpeg_search_paths}
  PATH_SUFFIXES lib lib64
)
find_library(AVDEVICE_SO avdevice
  HINTS ${_ffmpeg_search_paths}
  PATH_SUFFIXES lib lib64
)
find_library(AVFILTER_SO avfilter
  HINTS ${_ffmpeg_search_paths}
  PATH_SUFFIXES lib lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS FFMPEG_INCLUDE_DIR
                AVCODEC_SO
                AVFORMAT_SO
                AVUTIL_SO
                AVDEVICE_SO
                AVFILTER_SO
)

mark_as_advanced(
  FFMPEG_INCLUDE_DIR
  AVCODEC_SO
  AVFORMAT_SO
  AVUTIL_SO
  AVDEVICE_SO
  AVFILTER_SO
)

# Create imported targets
foreach(comp avcodec avformat avutil avdevice avfilter)
  string(TOUPPER ${comp} COMP_UPPER)
  find_library(LIB_${COMP_UPPER}_SO ${comp}
    HINTS ${_ffmpeg_search_paths}
    PATH_SUFFIXES lib lib64
  )
  add_library(FFmpeg::${comp} SHARED IMPORTED)
  set_target_properties(FFmpeg::${comp} PROPERTIES
    IMPORTED_LOCATION "${LIB_${COMP_UPPER}_SO}"
    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
  )
endforeach()

# 1. Create an INTERFACE library called ffmpeg_decode
add_library(ffmpeg_decoding INTERFACE)

# 2. Tell it to link against all the FFmpeg shared‐library targets
target_link_libraries(ffmpeg_decoding INTERFACE
    FFmpeg::avcodec
    FFmpeg::avformat
    FFmpeg::avutil
    FFmpeg::avdevice         # if you need device APIs
    FFmpeg::avfilter         # if you need filter APIs
)

# 3. (macOS only) If you still need to pull in VideoToolbox, wrap that too
if(APPLE)
  target_link_libraries(ffmpeg_decoding INTERFACE
    "-framework VideoToolbox"
    "-framework CoreFoundation"
    "-framework CoreMedia"
    "-framework CoreVideo"
  )
endif()

message(STATUS "Found FFmpeg lib, with include dirs: ${FFMPEG_INCLUDE_DIR}")
