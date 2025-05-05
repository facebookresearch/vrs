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


file(GLOB XPRS_SRCS *.cpp *.h *.hpp)
add_library(xprs ${XPRS_SRCS})
set(XPRS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# option for turn on / off HW decoder
option(ENABLE_NVCODEC "Enable NVCodec support" OFF)
if (ENABLE_NVCODEC)
  target_compile_options(xprs PRIVATE WITH_NVCODEC)
endif()

target_link_libraries(xprs
  PRIVATE
    vrs_logging
    ffmpeg_decoding
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
