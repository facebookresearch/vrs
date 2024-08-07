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

#include "FilterCopy.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>

#define DEFAULT_LOG_CHANNEL "FilterCopy"
#include <logging/Log.h>

using namespace std;
using namespace vrs;

namespace vrs::utils {

void copyMergeDoc() {
  cout << "When combining multiple VRS files into a single VRS file, the following rules apply:\n";
  cout << "\n";
  cout << "File tags will be merged. If a tag name is used in multiple file, the value found in\n";
  cout << "the first file is used, the others are ignored.\n";
  cout << "\n";
  cout << "The 'copy' option keeps streams separate, even when two streams found in different\n";
  cout << "source files have the same StreamId.\n";
  cout << "\n";
  cout << "The 'merge' option will merge streams with the same RecordableTypeId,\n";
  cout << "in their respective order in each source file. So for each RecordableTypeId:\n";
  cout << " - the first streams with that RecordableTypeId in each file are merged together,\n";
  cout << " - the second streams with that RecordableTypeId in each file are merged together,\n";
  cout << " - etc.\n";
  cout << "Stream tags are also merged, using the priority logic as for file tags.\n";
  cout << "\n";
  cout << "If the files don't have streams with matching RecordableTypeId, both copy and merge\n";
  cout << "operations produce the same output.\n";
  cout << "\n";
  cout << "Important: it's the RecordableTypeId that's matched, not the StreamId.\n";
  cout << "So if you stream-merge two files, each with a single stream, the streams will be\n";
  cout << "merged into a single stream if their RecordableTypeId is identical, regardless of\n";
  cout << "the streams instance ID.\n";
}

void printProgress(const char* status, size_t currentSize, size_t totalSize, bool showProgress) {
  if (showProgress) {
    size_t percent = 100 * currentSize / totalSize;
    cout << kResetCurrentLine << status << setw(2) << percent << "%...";
    cout.flush();
  }
}

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
    const MakeStreamFilterFunction& makeStreamFilter,
    unique_ptr<ThrottledFileDelegate> throttledFileDelegate) {
  ThrottledWriter throttledWriter(copyOptions, *throttledFileDelegate);
  RecordFileWriter& writer = throttledWriter.getWriter();
  writer.addTags(filteredReader.reader.getTags());
  filteredReader.reader.clearStreamPlayers();
  vector<unique_ptr<StreamPlayer>> filters;
  filters.reserve(filteredReader.filter.streams.size());
  {
    TemporaryRecordableInstanceIdsResetter resetter;
    for (auto id : filteredReader.filter.streams) {
      filters.emplace_back(makeStreamFilter(filteredReader.reader, writer, id, copyOptions));
    }
  }
  double startTimestamp{}, endTimestamp{};
  filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
  if (copyOptions.tagOverrider) {
    copyOptions.tagOverrider->overrideTags(writer);
  }
  if (throttledFileDelegate->shouldPreallocateIndex()) {
    writer.preallocateIndex(filteredReader.buildIndex());
  }
  int copyResult = throttledFileDelegate->createFile(pathToCopy);
  if (copyResult == 0) {
    // Init tracker progress early, to be sure we track the background thread queue size
    filteredReader.preRollConfigAndState(); // make sure to copy most recent config & state records
    throttledWriter.initTimeRange(startTimestamp, endTimestamp, &filteredReader.reader);
    filteredReader.iterateAdvanced(&throttledWriter);
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

namespace {

// given a list of existing tags and a list of new tags, create a list of tags to insert
void mergeTags(
    const map<string, string>& writtenTags,
    const map<string, string>& newTags,
    map<string, string>& outTags,
    const string& source,
    bool isVrsPrivate) {
  for (const auto& newTag : newTags) {
    auto writtenTag = writtenTags.find(newTag.first);
    // add tags, but don't overwrite an existing value (warn instead)
    if (writtenTag != writtenTags.end()) {
      if (newTag.second != writtenTag->second) {
        // if that tag is already set to a different value
        if (isVrsPrivate) {
          // don't merge private VRS tags, but warn...
          cerr << "Warning! The tag '" << newTag.first << "' was already set, ignoring value '"
               << newTag.second << "' from " << source << "\n";
        } else {
          // store value using a new name, to preserve (some) context
          cerr << "Warning! The tag '" << newTag.first
               << "' was already set. Dup found in: " << source << "\n";
          string newName = newTag.first + "_merged";
          int count = 1;
          // find a name that's not in use anywhere.
          // Because of collisions, we even need to check newTags & outTags...
          while (writtenTags.find(newName) != writtenTags.end() ||
                 newTags.find(newName) != newTags.end() || outTags.find(newName) != outTags.end()) {
            newName = newTag.first + "_merged-" + to_string(count++);
          }
          outTags[newName] = newTag.second;
        }
      } else {
        // the value is identical: do nothing.
      }
    } else {
      outTags[newTag.first] = newTag.second;
    }
  }
}

class NoDuplicateCopier : public Copier {
 public:
  NoDuplicateCopier(
      RecordFileReader& fileReader,
      RecordFileWriter& fileWriter,
      StreamId id,
      const CopyOptions& copyOptions)
      : Copier(fileReader, fileWriter, id, copyOptions) {
    lastRecordTimestamps_.fill(nan(""));
  }

  bool processRecordHeader(const CurrentRecord& record, DataReference& outDataRef) override {
    double& lastTimestamp = lastRecordTimestamps_[static_cast<size_t>(record.recordType)];
    if (lastTimestamp == record.timestamp) {
      return false;
    }
    lastTimestamp = record.timestamp;
    return Copier::processRecordHeader(record, outDataRef);
  }

 protected:
  array<double, static_cast<size_t>(Record::Type::COUNT)> lastRecordTimestamps_{};
};

struct SourceRecord {
  RecordFileReader* reader;
  const IndexRecord::RecordInfo* record;

  bool operator<(const SourceRecord& rhs) const {
    return *record < *rhs.record;
  }
};

} // namespace

int filterMerge(
    FilteredFileReader& firstRecordFilter,
    const vector<FilteredFileReader*>& moreRecordFilters,
    const string& pathToCopy,
    const CopyOptions& copyOptions,
    unique_ptr<ThrottledFileDelegate> throttledFileDelegate) {
  // setup the record file writer and hook-up the readers to record copiers, and copy/merge tags
  ThrottledWriter throttledWriter(copyOptions, *throttledFileDelegate);
  RecordFileWriter& recordFileWriter = throttledWriter.getWriter();
  firstRecordFilter.reader.clearStreamPlayers();
  list<NoDuplicateCopier> copiers; // to delete copiers on exit
  // track copiers by recordable type id in sequence/instance order in the output file
  map<RecordableTypeId, vector<NoDuplicateCopier*>> copiersMap;
  // copy the tags & create the copiers for the first source file
  recordFileWriter.addTags(firstRecordFilter.reader.getTags());
  for (auto id : firstRecordFilter.filter.streams) {
    copiers.emplace_back(firstRecordFilter.reader, recordFileWriter, id, copyOptions);
    copiersMap[id.getTypeId()].push_back(&copiers.back());
  }
  // calculate the overall timerange, so we can resolve time constraints on the overall file
  double startTimestamp{}, endTimestamp{};
  firstRecordFilter.getTimeRange(startTimestamp, endTimestamp);
  for (auto* recordFilter : moreRecordFilters) {
    RecordFileReader& reader = recordFilter->reader;
    reader.clearStreamPlayers();
    recordFilter->expandTimeRange(startTimestamp, endTimestamp);
    // add the global tags
    map<string, string> tags;
    mergeTags(
        recordFileWriter.getTags(),
        reader.getTags(),
        tags,
        recordFilter->spec.getEasyPath(),
        false);
    recordFileWriter.addTags(tags);

    // track how many stream of each type we've seen in the current reader
    map<RecordableTypeId, size_t> recordableIndex;
    // For each stream, see if we merge it into an existing stream, or create a new one
    for (auto id : recordFilter->filter.streams) {
      if (copyOptions.mergeStreams) {
        size_t index = recordableIndex[id.getTypeId()]++; // get index & increment for next one
        vector<NoDuplicateCopier*>& existingCopiers = copiersMap[id.getTypeId()];
        if (index < existingCopiers.size()) {
          // merge this stream: re-use the existing copier
          reader.setStreamPlayer(id, existingCopiers[index]);
          Writer& writer = existingCopiers[index]->getWriter();
          // merge new user & vrs tags into the existing stream tags
          const StreamTags& writtenTags = writer.getStreamTags();
          const StreamTags& newTags = reader.getTags(id);
          StreamTags streamTags;
          string tagSource = id.getName() + " of " + recordFilter->spec.getEasyPath();
          mergeTags(writtenTags.user, newTags.user, streamTags.user, tagSource, false);
          mergeTags(writtenTags.vrs, newTags.vrs, streamTags.vrs, tagSource, true);
          writer.addTags(streamTags);
        } else {
          copiers.emplace_back(reader, recordFileWriter, id, copyOptions);
          copiersMap[id.getTypeId()].push_back(&copiers.back());
        }
      } else {
        copiers.emplace_back(reader, recordFileWriter, id, copyOptions);
      }
    }
  }

  if (copyOptions.tagOverrider) {
    copyOptions.tagOverrider->overrideTags(throttledWriter.getWriter());
  }

  // create a time-sorted list of all the records (pre-flight only: no actual read)
  deque<SourceRecord> records;
  auto recordCollector = [&records](
                             RecordFileReader& reader, const IndexRecord::RecordInfo& record) {
    records.push_back(SourceRecord{&reader, &record});
    return true;
  };
  firstRecordFilter.filter.resolveRelativeTimeConstraints(startTimestamp, endTimestamp);
  firstRecordFilter.preRollConfigAndState(recordCollector);
  firstRecordFilter.iterateAdvanced(recordCollector);
  for (auto* recordFilter : moreRecordFilters) {
    recordFilter->filter.resolveRelativeTimeConstraints(startTimestamp, endTimestamp);
    recordFilter->preRollConfigAndState(recordCollector);
    recordFilter->iterateAdvanced(recordCollector);
  }
  sort(records.begin(), records.end());

  if (throttledFileDelegate->shouldPreallocateIndex()) {
    // Build preliminary index
    auto preliminaryIndex = make_unique<deque<IndexRecord::DiskRecordInfo>>();
    deque<IndexRecord::DiskRecordInfo>& index = *preliminaryIndex;
    int64_t offset = 0;
    for (auto& r : records) {
      index.emplace_back(
          r.record->timestamp,
          static_cast<uint32_t>(r.record->fileOffset - offset),
          r.record->streamId,
          r.record->recordType);
      offset = r.record->fileOffset;
    }
    recordFileWriter.preallocateIndex(std::move(preliminaryIndex));
  }

  int mergeResult = throttledFileDelegate->createFile(pathToCopy);
  if (mergeResult == 0) {
    // read all the records in order
    if (!records.empty()) {
      throttledWriter.initTimeRange(
          records.front().record->timestamp, records.back().record->timestamp);
      for (auto& recordSource : records) {
        recordSource.reader->readRecord(*recordSource.record);
        throttledWriter.onRecordDecoded(recordSource.record->timestamp);
      }
    }
    mergeResult = throttledFileDelegate->closeFile();
  }
  return mergeResult;
}

} // namespace vrs::utils
