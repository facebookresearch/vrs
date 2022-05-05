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

# - Find Png
# Find the Png library and includes
#
# Png_INCLUDE_DIRS - where to find png.h, etc.
# Png_LIBRARIES - List of libraries when using Png.
# Png_FOUND - True if Png found.

find_path(Png_INCLUDE_DIRS NAMES png.h)
find_library(Png_LIBRARIES NAMES png)
mark_as_advanced(Png_LIBRARIES Png_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Png DEFAULT_MSG Png_LIBRARIES Png_INCLUDE_DIRS)

if (Png_FOUND AND NOT (TARGET Png::Png))
  add_library(Png::Png UNKNOWN IMPORTED)
  set_target_properties(Png::Png
    PROPERTIES
      IMPORTED_LOCATION ${Png_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${Png_INCLUDE_DIRS}
  )
endif()
