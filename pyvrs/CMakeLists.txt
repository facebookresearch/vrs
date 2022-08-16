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

# Keep functionality to reader only for now
file (GLOB_RECURSE VRSBINDINGS_SRCS
  "vrs_bindings/Pybind11.cpp"
  "vrs_bindings/VrsBindings.cpp"
  "vrs_bindings/VrsBindings.h"
  "vrs_bindings/reader/*.cpp"
  "vrs_bindings/reader/*.h"
  "vrs_bindings/reader/*.hpp"
  "vrs_bindings/utils/*.cpp"
  "vrs_bindings/utils/*.h"
)

# Remove fb-only source files (if there is still any lying around)
file (GLOB_RECURSE VRSBINDINGS_FB_SRCS "vrs_bindings/*/*_fb\.*")
if (VRSBINDINGS_FB_SRCS)
  list(REMOVE_ITEM VRSBINDINGS_SRCS ${VRSBINDINGS_FB_SRCS})
endif()

message(STATUS "${VRSBINDINGS_SRCS}")

find_package(pybind11)
pybind11_add_module(vrsbindings ${VRSBINDINGS_SRCS})

target_include_directories(vrsbindings PUBLIC ${VRS_SOURCE_DIR}/pyvrs/vrs_bindings)
target_link_libraries(vrsbindings
  PUBLIC
    vrslib
    vrs_utils
)
