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

# - Find FFmpeg libraries

# Allow user override of root install prefix
if(NOT FFmpeg_ROOT)
  set(FFmpeg_ROOT "$ENV{HOME}/vrs_third_party_libs/ffmpeg")
endif()

# Headers
find_path(FFMPEG_INCLUDE_DIR
  NAMES libavcodec/avcodec.h
  HINTS ${FFmpeg_ROOT}
  PATH_SUFFIXES include
)

# Create imported targets
foreach(comp avcodec avformat avutil)
  string(TOUPPER ${comp} COMP_UPPER)
  find_library(${COMP_UPPER}_SO ${comp}
    HINTS ${FFmpeg_ROOT}
    PATH_SUFFIXES lib lib64
  )
  add_library(FFmpeg::${comp} SHARED IMPORTED)
  set_target_properties(FFmpeg::${comp} PROPERTIES
    IMPORTED_LOCATION "${${COMP_UPPER}_SO}"
    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
  )
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS FFMPEG_INCLUDE_DIR
                AVCODEC_SO
                AVFORMAT_SO
                AVUTIL_SO
)

# 1. Create an INTERFACE library called ffmpeg_decoding
add_library(ffmpeg_decoding INTERFACE)

# 2. Tell it to link against all the FFmpeg shared‐library targets
target_link_libraries(ffmpeg_decoding INTERFACE
    FFmpeg::avcodec
    FFmpeg::avformat
    FFmpeg::avutil
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
