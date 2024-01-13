/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ListRecords.h"

#include <fmt/format.h>

#include <vrs/ForwardDefinitions.h>
#include <vrs/StreamPlayer.h>

using namespace std;
using namespace vrs;

namespace {

class RecordLister : public StreamPlayer {
 public:
  bool processConfigurationHeader(const CurrentRecord& record, DataReference&) override {
    list(record);
    return false;
  }

  bool processStateHeader(const CurrentRecord& record, DataReference&) override {
    list(record);
    return false;
  }

  bool processDataHeader(const CurrentRecord& record, DataReference&) override {
    list(record);
    return false;
  }

  static void list(const CurrentRecord& record) {
    fmt::print(
        "{:.3f} {} [{}], {} record, {} bytes total.\n",
        record.timestamp,
        record.streamId.getName(),
        record.streamId.getNumericName(),
        Record::typeName(record.recordType),
        record.recordSize);
  }
};

} // namespace

namespace vrs::utils {

void listRecords(utils::FilteredFileReader& filteredReader) {
  filteredReader.reader.clearStreamPlayers();
  RecordLister lister;
  for (auto id : filteredReader.filter.streams) {
    filteredReader.reader.setStreamPlayer(id, &lister);
  }
  // this option doesn't use RecordFormat, and it's only a record list: no need to preroll at all!
  double startTimestamp{}, endTimestamp{};
  filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
  filteredReader.iterateAdvanced();
}

} // namespace vrs::utils
