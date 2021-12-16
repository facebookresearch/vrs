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

set(SAMPLE_PLAYBACK_APP_SRCS "SamplePlaybackApp.cpp")
add_executable(sample_playback_app ${SAMPLE_PLAYBACK_APP_SRCS})
target_link_libraries(sample_playback_app
    PUBLIC
        vrs
        vrs_logging
        vrs_os
)


set(SAMPLE_RECORDING_APP_SRCS "SampleRecordingApp.cpp")
add_executable(sample_recording_app ${SAMPLE_RECORDING_APP_SRCS})
target_link_libraries(sample_recording_app
    PUBLIC
        vrs
        vrs_logging
        vrs_os
)
