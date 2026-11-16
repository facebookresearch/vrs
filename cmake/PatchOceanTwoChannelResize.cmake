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

set(source_file "${OCEAN_SOURCE_DIR}/impl/ocean/cv/FrameInterpolatorBilinear.cpp")
file(READ "${source_file}" source)
string(REGEX MATCH
  "case 2u:[ \t\r\n]+rowInterpolationHorizontalFunction = interpolateRowHorizontal8BitPerChannel7BitPrecision;"
  scalar_dispatch "${source}"
)
if(NOT scalar_dispatch STREQUAL "")
  return()
endif()
string(REGEX MATCH
  "case 2u:[ \t\r\n]+rowInterpolationHorizontalFunction = interpolateRowHorizontal8BitPerChannel7BitPrecisionNEON<2u>;"
  neon_dispatch "${source}"
)
if(neon_dispatch STREQUAL "")
  message(FATAL_ERROR "Unexpected Ocean two-channel horizontal interpolation dispatch")
endif()
string(REPLACE "PrecisionNEON<2u>;" "Precision;" scalar_dispatch "${neon_dispatch}")
string(REPLACE "${neon_dispatch}" "${scalar_dispatch}" patched_source "${source}")
file(WRITE "${source_file}" "${patched_source}")
