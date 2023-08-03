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

# Start from an ubuntu container
FROM ubuntu:jammy

# Get dependencies
RUN apt-get update && DEBIAN_FRONTEND="noninteractive" TZ="America/New_York" apt-get install -y tzdata
RUN apt-get install -y git cmake ninja-build ccache libgtest-dev libfmt-dev libturbojpeg-dev\
    libpng-dev liblz4-dev libzstd-dev libxxhash-dev\
    libboost-system-dev libboost-filesystem-dev libboost-thread-dev libboost-chrono-dev libboost-date-time-dev\
    qtbase5-dev portaudio19-dev\
    npm doxygen;

# Code
ADD ./ /opt/vrs

# Configure
RUN mkdir /opt/vrs_Build; cd /opt/vrs_Build;  cmake -DCMAKE_BUILD_TYPE=RELEASE -S /opt/vrs -G Ninja -Wno-dev;

# Build & test
RUN cd /opt/vrs_Build; ninja all; ctest -j;
