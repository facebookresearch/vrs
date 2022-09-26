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

find_package(Boost REQUIRED
  COMPONENTS
    filesystem
    chrono
    date_time
    system
    thread
)
find_package(FmtLib REQUIRED)
find_package(Cereal REQUIRED)
find_package(Lz4 REQUIRED)
find_package(Zstd REQUIRED)
find_package(xxHash REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(TurboJpeg REQUIRED)

# Setup unit test infra, but only if unit tests are enabled
if (UNIT_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
endif()
