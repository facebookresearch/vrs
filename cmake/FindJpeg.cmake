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
# - Find Jpeg
# Find the Jpeg compression library and includes
#
# Jpeg_INCLUDE_DIRS - where to find jpeglib.h, etc.
# Jpeg_LIBRARIES - List of libraries when using Jpeg.
# Jpeg_FOUND - True if Jpeg found.

find_path(Jpeg_INCLUDE_DIRS NAMES jpeglib.h)
find_library(Jpeg_LIBRARIES NAMES jpeg)
mark_as_advanced(Jpeg_LIBRARIES Jpeg_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jpeg DEFAULT_MSG Jpeg_LIBRARIES Jpeg_INCLUDE_DIRS)

if (Jpeg_FOUND AND NOT (TARGET Jpeg::Jpeg))
  add_library(Jpeg::Jpeg UNKNOWN IMPORTED)
  set_target_properties(Jpeg::Jpeg
    PROPERTIES
      IMPORTED_LOCATION ${Jpeg_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${Jpeg_INCLUDE_DIRS})
endif()
