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

# This library is a minimal replacement for the TestDataDir library used at Meta.

file (GLOB VRS_OSS_TESTDATADIR *.cpp *.h)

add_library(vrs_oss_testdatadir STATIC ${VRS_OSS_TESTDATADIR})
target_include_directories(vrs_oss_testdatadir PUBLIC ${VRS_SOURCE_DIR}/vrs/oss)
target_link_libraries(vrs_oss_testdatadir
  PUBLIC
    vrs_os)
target_compile_definitions(vrs_oss_testdatadir PRIVATE VRS_SOURCE_DIR="${VRS_SOURCE_DIR}")
