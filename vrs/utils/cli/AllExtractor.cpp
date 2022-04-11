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

#include "AllExtractor.h"

#define DEFAULT_LOG_CHANNEL "AllExtractor"
#include <logging/Log.h>

#include <vrs/RecordFileReader.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/utils/DataExtractor.h>

using namespace std;

namespace vrs::utils {

int extractAll(const string& outputFolder, FilteredFileReader& filteredReader) {
  utils::DataExtractor extractor(filteredReader.reader, outputFolder);
  for (auto id : filteredReader.filter.streams) {
    extractor.extract(id);
  }
  IF_ERROR_LOG_AND_RETURN(extractor.createOutput());
  filteredReader.iterateSafe();
  return extractor.completeOutput();
}

} // namespace vrs::utils
