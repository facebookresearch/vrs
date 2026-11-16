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

if(NOT DEFINED TEST_ROOT)
  message(FATAL_ERROR "Pass -DTEST_ROOT=<temporary directory>")
endif()
set(patch_script "${CMAKE_CURRENT_LIST_DIR}/../PatchOceanTwoChannelResize.cmake")
set(source_dir "${TEST_ROOT}/ocean")
set(source_file "${source_dir}/impl/ocean/cv/FrameInterpolatorBilinear.cpp")
file(MAKE_DIRECTORY "${source_dir}/impl/ocean/cv")
set(neon_source [=[switch (channels)
{
  case 1u:
    rowInterpolationHorizontalFunction = interpolateRowHorizontal8BitPerChannel7BitPrecisionNEON<1u>;
    break;
  case 2u:
    rowInterpolationHorizontalFunction = interpolateRowHorizontal8BitPerChannel7BitPrecisionNEON<2u>;
    break;
  default:
    rowInterpolationHorizontalFunction = interpolateRowHorizontal8BitPerChannel7BitPrecision;
    break;
}
]=])
set(scalar_source [=[switch (channels)
{
  case 1u:
    rowInterpolationHorizontalFunction = interpolateRowHorizontal8BitPerChannel7BitPrecisionNEON<1u>;
    break;
  case 2u:
    rowInterpolationHorizontalFunction = interpolateRowHorizontal8BitPerChannel7BitPrecision;
    break;
  default:
    rowInterpolationHorizontalFunction = interpolateRowHorizontal8BitPerChannel7BitPrecision;
    break;
}
]=])
file(WRITE "${source_file}" "${neon_source}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DOCEAN_SOURCE_DIR=${source_dir}" -P "${patch_script}"
  RESULT_VARIABLE result
)
file(READ "${source_file}" actual)
if(NOT result EQUAL 0 OR NOT actual STREQUAL scalar_source)
  message(FATAL_ERROR "Patch must change only the two-channel dispatch to scalar")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DOCEAN_SOURCE_DIR=${source_dir}" -P "${patch_script}"
  RESULT_VARIABLE result
)
file(READ "${source_file}" actual)
if(NOT result EQUAL 0 OR NOT actual STREQUAL scalar_source)
  message(FATAL_ERROR "Applying the patch twice must leave the source unchanged")
endif()
file(WRITE "${source_file}" "case 2u:\n  rowInterpolationHorizontalFunction = unknownInterpolator;\n")
file(READ "${source_file}" unexpected_source)
execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DOCEAN_SOURCE_DIR=${source_dir}" -P "${patch_script}"
  RESULT_VARIABLE result
  ERROR_VARIABLE error
)
file(READ "${source_file}" actual)
if(result EQUAL 0 OR NOT actual STREQUAL unexpected_source OR
   NOT error MATCHES "Unexpected Ocean two-channel horizontal interpolation dispatch")
  message(FATAL_ERROR "Unexpected upstream dispatch must fail without changing the source")
endif()
message(STATUS "Ocean two-channel resize patch tests passed")
