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

find_package(fmt)

if (NOT TARGET fmt::fmt)
  find_path(Fmt_INCLUDE_DIR fmt/core.h)
  mark_as_advanced(Fmt_INCLUDE_DIR)

  find_library(Fmt_LIBRARY NAMES fmt fmtd)
  mark_as_advanced(Fmt_LIBRARY)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    fmt
    DEFAULT_MSG
    Fmt_LIBRARY Fmt_INCLUDE_DIR
  )
  set(Fmt_LIBRARIES ${Fmt_LIBRARY})
  set(Fmt_INCLUDE_DIRS ${Fmt_INCLUDE_DIR})
  add_library(fmt::fmt UNKNOWN IMPORTED)
  set_target_properties(
    fmt::fmt PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${Fmt_INCLUDE_DIR}"
  )
  set_target_properties(
    fmt::fmt PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${Fmt_LIBRARY}"
  )
endif()
