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
#
# - Find Zstd
# Find the Zstd compression library and includes
#
# Zstd_INCLUDE_DIRS - where to find Zstd.h, etc.
# Zstd_LIBRARIES - List of libraries when using Zstd.
# Zstd_FOUND - True if Zstd found.

find_path(Zstd_INCLUDE_DIRS NAMES zstd.h)
find_library(Zstd_LIBRARIES NAMES zstd)
mark_as_advanced(Zstd_LIBRARIES Zstd_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Zstd DEFAULT_MSG Zstd_LIBRARIES Zstd_INCLUDE_DIRS)

if (Zstd_FOUND AND NOT (TARGET Zstd::Zstd))
  add_library(Zstd::Zstd UNKNOWN IMPORTED)
  set_target_properties(Zstd::Zstd
    PROPERTIES
      IMPORTED_LOCATION ${Zstd_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${Zstd_INCLUDE_DIRS}
  )
endif()
