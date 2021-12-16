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

find_path(xxHash_INCLUDE_DIRS
  NAME xxhash.h
  PATH_SUFFIXES include)
find_library(xxHash_LIBRARIES
  NAMES xxhash xxHash
  PATH_SUFFIXES lib)
mark_as_advanced(xxHash_LIBRARIES xxHash_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(xxHash DEFAULT_MSG xxHash_LIBRARIES xxHash_INCLUDE_DIRS)

if (xxHash_FOUND AND NOT (TARGET xxHash::xxHash))
  add_library(xxHash::xxHash UNKNOWN IMPORTED)
  set_target_properties(xxHash::xxHash
    PROPERTIES
      IMPORTED_LOCATION ${xxHash_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${xxHash_INCLUDE_DIRS})
endif()
