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

#include "ImageExtraction.h"

#include <fmt/format.h>

#include <vrs/os/Utils.h>
#include <vrs/utils/ImageExtractor.h>

using namespace std;

namespace vrs::utils {

void extractImages(const string& path, FilteredFileReader& filteredReader, bool extractImagesRaw) {
  if (path.length() > 0) {
    os::makeDirectories(path);
  }
  uint32_t imageCounter = 0;
  deque<unique_ptr<StreamPlayer>> extractors;
  for (auto id : filteredReader.filter.streams) {
    if (filteredReader.reader.mightContainImages(id)) {
      extractors.emplace_back(make_unique<ImageExtractor>(path, imageCounter, extractImagesRaw));
      filteredReader.reader.setStreamPlayer(id, extractors.back().get());
    }
  }
  filteredReader.iterateSafe();
  cout << "Found " << imageCounter << " image(s)." << endl;
}

} // namespace vrs::utils
