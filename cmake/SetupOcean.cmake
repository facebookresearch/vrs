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

include(FetchContent) # once in the project to include the module

FetchContent_Declare(
  ocean
  GIT_REPOSITORY      https://github.com/facebookresearch/ocean.git
  GIT_TAG             origin/main
)

# Override config for Ocean
set(OCEAN_BUILD_MINIMAL ON CACHE INTERNAL "Build minimal ocean lib")
set(OCEAN_BUILD_TEST OFF CACHE INTERNAL "Skip building ocean test")

message("Pulling deps: {ocean}")
FetchContent_MakeAvailable(ocean)
