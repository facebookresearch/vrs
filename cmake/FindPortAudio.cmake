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

# - Find PortAudio
# Find the PortAudio compression library and includes
#
# PortAudio_INCLUDE_DIRS - where to find PortAudio.h, etc.
# PortAudio_LIBRARIES - List of libraries when using PortAudio.
# PortAudio_FOUND - True if PortAudio found.

find_path(PortAudio_INCLUDE_DIRS NAMES portaudio.h)
find_library(PortAudio_LIBRARIES NAMES portaudio)
mark_as_advanced(PortAudio_LIBRARIES PortAudio_INCLUDE_DIRS)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PortAudio DEFAULT_MSG PortAudio_LIBRARIES PortAudio_INCLUDE_DIRS)

if (PortAudio_FOUND AND NOT (TARGET PortAudio::PortAudio))
  add_library(PortAudio::PortAudio UNKNOWN IMPORTED)
  set_target_properties(PortAudio::PortAudio
    PROPERTIES
      IMPORTED_LOCATION ${PortAudio_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${PortAudio_INCLUDE_DIRS}
  )
endif()
