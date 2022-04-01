// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <vrs/utils/FilteredFileReader.h>

namespace vrs::utils {

/// Helper to extract raw audio streams from a VRS file into WAV files.
/// Streams containing no audio will be ignored.
/// @param folderPath: path to a folder where to extract the files
/// @param filteredReader: filtered reader for the file to read from.
/// @return A status code, 0 meaning success.
int extractAudio(const std::string& folderPath, FilteredFileReader& filteredReader);

} // namespace vrs::utils
