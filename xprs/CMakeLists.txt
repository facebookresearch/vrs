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


# Base sources (always built). Explicit list rather than file(GLOB) so the
# NVDEC/CUDA sources (nvDecoder.cpp, cudaContextProvider.cpp) are not compiled
# unconditionally: they need nv-codec-headers (ffnvcodec) at build time and an
# NVIDIA driver at runtime, so the default OSS build (ENABLE_NVCODEC=OFF) must
# leave them out. Only the files synced to OSS by vrs.cconf are listed.
#
# DECODER-ONLY: the OSS distribution ships only the decode path. The encoder
# sources (nvEncoder.cpp, FFmpegEncode, etc.) depend on internal-only headers
# that are not synced to OSS; xprsEncZero.cpp is the no-op encoder stub that
# lets the library link without them.
set(XPRS_SRCS
  FFmpegDecode.cpp
  FFmpegUtils.cpp
  xprsBase.cpp
  xprsDecApi.cpp
  xprsDecoder.cpp
  xprsEncZero.cpp
  xprsUtils.cpp
)

# Headers — kept in the library for IDE/install visibility.
set(XPRS_HEADERS
  Codecs.h
  FFmpegDecode.h
  FFmpegUtils.h
  Final.h
  InternalDecoder.h
  xprs.h
  xprsDecoder.h
  xprsUtils.h
)

# NVDEC/CUDA decode sources (only when ENABLE_NVCODEC=ON).
set(XPRS_NVCODEC_SRCS
  nvDecoder.cpp
  cudaContextProvider.cpp
)

option(ENABLE_NVCODEC "Enable NVIDIA CUVID/NVDEC GPU-accelerated H.265 decode" OFF)

if (ENABLE_NVCODEC)
  list(APPEND XPRS_SRCS ${XPRS_NVCODEC_SRCS})
  list(APPEND XPRS_HEADERS
    cudaContextProvider.h
    cudaUtils.h
    nvDecoder.h
  )
endif()

add_library(xprs ${XPRS_SRCS} ${XPRS_HEADERS})
set(XPRS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if (ENABLE_NVCODEC)
  target_compile_definitions(xprs PRIVATE WITH_NVCODEC=1)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFNVCODEC REQUIRED IMPORTED_TARGET ffnvcodec)
  target_link_libraries(xprs PRIVATE PkgConfig::FFNVCODEC)
endif()

target_link_libraries(xprs
  PUBLIC
    ffmpeg_decoding
  PRIVATE
    vrs_logging
)

target_include_directories(xprs
  PUBLIC
    $<BUILD_INTERFACE:${VRS_SOURCE_DIR}/xprs>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS xprs ffmpeg_decoding EXPORT VRSLibTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)
