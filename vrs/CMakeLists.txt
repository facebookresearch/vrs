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

add_subdirectory(helpers)
add_subdirectory(os)
add_subdirectory(oss)
add_subdirectory(utils)

file (GLOB VRS_SRCS *.cpp *.h *.hpp)
add_library(vrslib ${VRS_SRCS})
target_include_directories(vrslib
  PUBLIC
    $<BUILD_INTERFACE:${VRS_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(vrslib
  PUBLIC
    vrs_helpers
    vrs_logging
    vrs_os
    vrs_utils_xxhash
    Lz4::Lz4
    Zstd::Zstd
)

install(TARGETS vrslib EXPORT VRSLibTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(EXPORT VRSLibTargets
  NAMESPACE vrs::
  FILE vrslibTargets.cmake
  DESTINATION lib/cmake/vrslib
)

include(CMakePackageConfigHelpers)

configure_package_config_file(vrslibConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/vrslibConfig.cmake
  INSTALL_DESTINATION ${LIB_INSTALL_DIR}/vrslib/cmake
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/vrslibConfigVersion.cmake"
  VERSION "1.0"
  COMPATIBILITY AnyNewerVersion
)

install(
  DIRECTORY .
  DESTINATION include/vrs
  COMPONENT headers
  FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(
  DIRECTORY ../cmake
  DESTINATION lib/cmake/vrslib
  FILES_MATCHING PATTERN "*.cmake"
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/vrslibConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/vrslibConfigVersion.cmake"
  DESTINATION lib/cmake/vrslib
)

if (UNIT_TESTS)
  add_subdirectory(test)
endif()
