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

#include "FilterCopy.h"

#define DEFAULT_LOG_CHANNEL "FilterCopy"
#include <logging/Log.h>

using namespace std;
using namespace vrs;

namespace vrs::utils {

unique_ptr<StreamPlayer> makeCopier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId streamId,
    const CopyOptions& copyOptions) {
  return make_unique<Copier>(fileReader, fileWriter, streamId, copyOptions);
}

int filterCopy(
    FilteredFileReader& filteredReader,
    const string& pathToCopy,
    const CopyOptions& copyOptions,
    MakeStreamFilterFunction makeStreamFilter,
    unique_ptr<ThrottledFileDelegate> throttledFileDelegate) {
  ThrottledWriter throttledWriter(copyOptions, *throttledFileDelegate);
  RecordFileWriter& writer = throttledWriter.getWriter();
  writer.addTags(filteredReader.reader.getTags());
  vector<unique_ptr<StreamPlayer>> filters;
  filters.reserve(filteredReader.filter.streams.size());
  size_t maxRecordCount = 0;
  for (auto id : filteredReader.filter.streams) {
    filters.emplace_back(makeStreamFilter(filteredReader.reader, writer, id, copyOptions));
    maxRecordCount += filteredReader.reader.getIndex(id).size();
  }
  double startTimestamp, endTimestamp;
  filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
  copyOptions.tagOverrides.overrideTags(writer);
  if (throttledFileDelegate->shouldPreallocateIndex()) {
    writer.preallocateIndex(filteredReader.buildIndex());
  }
  int copyResult = throttledFileDelegate->createFile(pathToCopy);
  if (copyResult == 0) {
    // Init tracker propgress early, to be sure we track the background thread queue size
    filteredReader.preRollConfigAndState(); // make sure to copy most recent config & state records
    throttledWriter.initTimeRange(startTimestamp, endTimestamp);
    filteredReader.iterate(&throttledWriter);
    for (auto& filter : filters) {
      filter->flush();
    }
    copyResult = throttledFileDelegate->closeFile();
    if (writer.getBackgroundThreadQueueByteSize() != 0) {
      XR_LOGE("Unexpected count of bytes left in queue after copy!");
    }
  }
  return copyResult;
}

} // namespace vrs::utils
