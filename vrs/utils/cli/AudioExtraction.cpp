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

#include "AudioExtraction.h"

#define DEFAULT_LOG_CHANNEL "AudioExtraction"
#include <logging/Log.h>

#include <vrs/os/Utils.h>
#include <vrs/utils/AudioExtractor.h>

using namespace std;
using namespace vrs;

namespace vrs::utils {

int extractAudio(const string& path, FilteredFileReader& filteredReader) {
  if (path.length() > 0) {
    if (!os::pathExists(path)) {
      int status = os::makeDirectories(path);
      if (status != 0) {
        XR_LOGE("Can't create output directory at {}", path);
        return status;
      }
    }
    if (!os::isDir(path)) {
      XR_LOGE("Can't write output files at {}, because something is there...", path);
      return FAILURE;
    }
  }
  uint32_t audioFileCount = 0;
  uint32_t streamCount = 0;
  deque<unique_ptr<StreamPlayer>> extractors;
  for (auto id : filteredReader.filter.streams) {
    if (filteredReader.reader.mightContainAudio(id)) {
      extractors.emplace_back(move(new AudioExtractor(path, id, audioFileCount)));
      filteredReader.reader.setStreamPlayer(id, extractors.back().get());
      ++streamCount;
    }
  }
  filteredReader.iterateSafe();
  cout << "Wrote " << audioFileCount << " audio file(s) from " << streamCount << " stream(s)."
       << endl;
  return 0;
}

} // namespace vrs::utils
