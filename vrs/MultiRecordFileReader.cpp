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

#include <algorithm>
#include <cstddef>
#include <queue>
#include <utility>

#define DEFAULT_LOG_CHANNEL "MultiRecordFileReader"
#include <logging/Checks.h>
#include <logging/Log.h>
#include <vrs/ErrorCode.h>
#include <vrs/MultiRecordFileReader.h>
#include <vrs/RecordFileReader.h>
#include <vrs/StreamId.h>
#include <vrs/TagConventions.h>
#include <vrs/helpers/FileMacros.h>

using namespace std;

using UniqueStreamId = vrs::MultiRecordFileReader::UniqueStreamId;

namespace vrs {

/// Checks whether a given record belogs to the given reader
static bool belongsTo(const IndexRecord::RecordInfo* record, const RecordFileReader& reader) {
  const auto& index = reader.getIndex();
  return index.size() > 0 && record >= &index[0] && record <= &index[index.size() - 1];
}

static bool timestampLT(const IndexRecord::RecordInfo* lhs, double rhsTimestamp) {
  return lhs->timestamp < rhsTimestamp;
}

int MultiRecordFileReader::open(const std::vector<std::string>& paths) {
  std::vector<FileSpec> fileSpecs;
  fileSpecs.reserve(paths.size());
  for (const auto& path : paths) {
    FileSpec fileSpec;
    IF_ERROR_RETURN(RecordFileReader::vrsFilePathToFileSpec(path, fileSpec));
    fileSpecs.push_back(fileSpec);
  }
  return open(fileSpecs);
}

int MultiRecordFileReader::open(const std::vector<FileSpec>& fileSpecs) {
  if (fileSpecs.empty()) {
    XR_LOGE("At least one file must be opened");
    return INVALID_REQUEST;
  }
  if (!readers_.empty()) {
    XR_LOGE("open() must be invoked only once per instance");
    return INVALID_REQUEST;
  }
  readers_.reserve(fileSpecs.size());
  for (const auto& fileSpec : fileSpecs) {
    readers_.push_back(make_unique<RecordFileReader>());
    auto& reader = readers_.back();
    const auto status = reader->openFile(fileSpec);
    if (status != SUCCESS) {
      close();
      return status;
    }
    const auto path = fileSpec.getEasyPath();
    filePaths_.push_back(path);
    XR_LOGD("Opened file '{}' and assigned to reader #{}", path, readers_.size() - 1);
  }
  if (!areFilesRelated()) {
    close();
    return INVALID_REQUEST;
  }
  initializeUniqueStreamIds();
  createConsolidatedIndex();
  initializeFileTags();
  isOpened_ = true;
  return SUCCESS;
}

int MultiRecordFileReader::close() {
  if (!isOpened_) {
    return NO_FILE_OPEN;
  }
  int resultFinal = SUCCESS;
  for (auto& reader : readers_) {
    auto result = reader->closeFile();
    if (resultFinal == SUCCESS) {
      resultFinal = result;
    }
  }
  readers_.clear();
  recordIndex_ = nullptr;
  uniqueStreamIds_.clear();
  readerStreamIdToUniqueMap_.clear();
  uniqueToStreamIdReaderPairMap_.clear();
  filePaths_.clear();
  fileTags_.clear();
  isOpened_ = false;
  return resultFinal;
}

const set<UniqueStreamId>& MultiRecordFileReader::getStreams() const {
  if (!isOpened_) {
    static const set<UniqueStreamId> emptyStreamsSet;
    return emptyStreamsSet;
  }
  if (hasSingleFile()) {
    return readers_.front()->getStreams();
  }
  return uniqueStreamIds_;
}

MultiRecordFileReader::~MultiRecordFileReader() {
  close();
}

uint32_t MultiRecordFileReader::getRecordCount() const {
  if (!isOpened_) {
    return 0;
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecordCount();
  }
  return XR_DEV_PRECONDITION_NOTNULL(recordIndex_)->size();
}

uint32_t MultiRecordFileReader::getRecordCount(UniqueStreamId uniqueStreamId) const {
  if (!isOpened_) {
    return 0;
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecordCount(uniqueStreamId);
  }
  const auto* streamReaderPair = getStreamIdReaderPair(uniqueStreamId);
  if (streamReaderPair == nullptr) {
    return 0;
  }
  const auto& streamId = streamReaderPair->first;
  const auto& reader = streamReaderPair->second;
  return reader->getRecordCount(streamId);
}

uint32_t MultiRecordFileReader::getRecordCount(
    UniqueStreamId uniqueStreamId,
    Record::Type recordType) const {
  if (!isOpened_) {
    return 0;
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecordCount(uniqueStreamId, recordType);
  }
  const auto* streamReaderPair = getStreamIdReaderPair(uniqueStreamId);
  if (streamReaderPair == nullptr) {
    return 0;
  }
  const auto& streamId = streamReaderPair->first;
  const auto& reader = streamReaderPair->second;
  return reader->getRecordCount(streamId, recordType);
}

const StreamTags& MultiRecordFileReader::getTags(UniqueStreamId uniqueStreamId) const {
  static const StreamTags sEmptyRecordableTags;
  if (!isOpened_) {
    return sEmptyRecordableTags;
  }
  if (hasSingleFile()) {
    return readers_.front()->getTags(uniqueStreamId);
  }
  const auto* streamReaderPair = getStreamIdReaderPair(uniqueStreamId);
  if (streamReaderPair == nullptr) {
    return sEmptyRecordableTags;
  }
  const auto& streamId = streamReaderPair->first;
  const auto& reader = streamReaderPair->second;
  return reader->getTags(streamId);
}

vector<std::pair<string, int64_t>> MultiRecordFileReader::getFileChunks() const {
  if (!isOpened_) {
    static const vector<std::pair<string, int64_t>> emptyVector;
    return emptyVector;
  }
  if (hasSingleFile()) {
    return readers_.front()->getFileChunks();
  }
  vector<std::pair<string, int64_t>> fileChunks;
  fileChunks.reserve(readers_.size());
  for (size_t i = 0; i < readers_.size(); i++) {
    fileChunks.emplace_back(filePaths_[i], readers_[i]->getTotalSourceSize());
  }
  return fileChunks;
}

const string& MultiRecordFileReader::getFlavor(UniqueStreamId streamId) const {
  return getTag(getTags(streamId).vrs, Recordable::getFlavorTagName());
}

vector<UniqueStreamId> MultiRecordFileReader::getStreams(
    RecordableTypeId typeId,
    const string& flavor) const {
  if (hasSingleFile()) {
    return readers_.front()->getStreams(typeId, flavor);
  }
  vector<UniqueStreamId> streamIds;
  for (const auto& streamId : uniqueStreamIds_) {
    if ((typeId == RecordableTypeId::Undefined || streamId.getTypeId() == typeId) &&
        (flavor.empty() || getFlavor(streamId) == flavor)) {
      streamIds.emplace_back(streamId);
    }
  }
  return streamIds;
}

UniqueStreamId MultiRecordFileReader::getStreamForTag(
    const string& tagName,
    const string& tag,
    RecordableTypeId typeId) const {
  if (!isOpened_) {
    return {};
  }
  if (hasSingleFile()) {
    return readers_.front()->getStreamForTag(tagName, tag, typeId);
  }
  for (const auto& streamId : uniqueStreamIds_) {
    if ((typeId == RecordableTypeId::Undefined || streamId.getTypeId() == typeId) &&
        getTag(streamId, tagName) == tag) {
      return streamId;
    }
  }
  return {};
}

uint32_t MultiRecordFileReader::getRecordIndex(const IndexRecord::RecordInfo* record) const {
  if (!isOpened_ || record == nullptr) {
    return getRecordCount();
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecordIndex(record);
  }
  if (getReader(record) == nullptr) {
    // Weeding out illegal records (which don't belong to any of the underlying readers)
    return getRecordCount();
  }
  auto lowerIt = std::lower_bound(
      recordIndex_->begin(),
      recordIndex_->end(),
      record,
      [this](const IndexRecord::RecordInfo* lhs, const IndexRecord::RecordInfo* rhs) {
        return this->timeLessThan(lhs, rhs);
      });
  if (lowerIt != recordIndex_->end() && *lowerIt == record) {
    return lowerIt - recordIndex_->begin();
  } else {
    return getRecordCount();
  }
}

const IndexRecord::RecordInfo* MultiRecordFileReader::getRecord(uint32_t index) const {
  if (!isOpened_) {
    return nullptr;
  }
  if (hasSingleFile()) {
    const auto& singleFileIndex = readers_.front()->getIndex();
    return index < singleFileIndex.size() ? &singleFileIndex[index] : nullptr;
  }
  return index < recordIndex_->size() ? (*recordIndex_)[index] : nullptr;
}

const IndexRecord::RecordInfo* MultiRecordFileReader::getRecord(
    UniqueStreamId streamId,
    uint32_t indexNumber) const {
  if (!isOpened_) {
    return nullptr;
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecord(streamId, indexNumber);
  }
  const vector<const IndexRecord::RecordInfo*>& streamIndex = getIndex(streamId);
  return indexNumber < streamIndex.size() ? streamIndex[indexNumber] : nullptr;
}

const IndexRecord::RecordInfo* MultiRecordFileReader::getRecord(
    UniqueStreamId streamId,
    Record::Type recordType,
    uint32_t indexNumber) const {
  if (!isOpened_) {
    return nullptr;
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecord(streamId, recordType, indexNumber);
  }
  const StreamIdReaderPair* streamIdReaderPair = getStreamIdReaderPair(streamId);
  if (streamIdReaderPair == nullptr) {
    return nullptr;
  }
  const RecordFileReader* reader = streamIdReaderPair->second;
  return reader->getRecord(streamIdReaderPair->first, recordType, indexNumber);
}

const IndexRecord::RecordInfo* MultiRecordFileReader::getLastRecord(
    UniqueStreamId streamId,
    Record::Type recordType) const {
  const auto& index = getIndex(streamId);
  for (auto iter = index.rbegin(); iter != index.rend(); ++iter) {
    if ((*iter)->recordType == recordType) {
      return *iter;
    }
  }
  return nullptr;
}

const vector<const IndexRecord::RecordInfo*>& MultiRecordFileReader::getIndex(
    UniqueStreamId streamId) const {
  static const vector<const IndexRecord::RecordInfo*> sEmptyIndex;
  if (!isOpened_) {
    return sEmptyIndex;
  }
  if (hasSingleFile()) {
    return readers_.front()->getIndex(streamId);
  }
  const StreamIdReaderPair* streamIdReaderPair = getStreamIdReaderPair(streamId);
  if (streamIdReaderPair == nullptr) {
    return sEmptyIndex;
  }
  const RecordFileReader* reader = streamIdReaderPair->second;
  return reader->getIndex(streamIdReaderPair->first);
}

void MultiRecordFileReader::setStreamPlayer(UniqueStreamId streamId, StreamPlayer* streamPlayer) {
  if (!isOpened_) {
    return;
  }
  if (hasSingleFile()) {
    readers_.front()->setStreamPlayer(streamId, streamPlayer);
  }
  const StreamIdReaderPair* streamIdReaderPair = getStreamIdReaderPair(streamId);
  if (streamIdReaderPair != nullptr) {
    RecordFileReader* reader = streamIdReaderPair->second;
    reader->setStreamPlayer(streamIdReaderPair->first, streamPlayer);
  }
}

uint32_t MultiRecordFileReader::getRecordFormats(
    UniqueStreamId streamId,
    RecordFormatMap& outFormats) const {
  outFormats.clear();
  if (!isOpened_) {
    return outFormats.size();
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecordFormats(streamId, outFormats);
  }
  const StreamIdReaderPair* streamIdReaderPair = getStreamIdReaderPair(streamId);
  if (streamIdReaderPair != nullptr) {
    RecordFileReader* reader = streamIdReaderPair->second;
    return reader->getRecordFormats(streamIdReaderPair->first, outFormats);
  }
  return outFormats.size();
}

int MultiRecordFileReader::readRecord(const IndexRecord::RecordInfo& recordInfo) {
  if (!isOpened_) {
    XR_LOGE("No file open");
    return NO_FILE_OPEN;
  }
  RecordFileReader* reader = getReader(&recordInfo);
  if (reader == nullptr) {
    XR_LOGE("Invalid recordInfo");
    return INVALID_PARAMETER;
  }
  return reader->readRecord(recordInfo);
}

bool MultiRecordFileReader::setCachingStrategy(CachingStrategy cachingStrategy) {
  if (!isOpened_) {
    return false;
  }
  for (auto& reader : readers_) {
    if (!reader->setCachingStrategy(cachingStrategy)) {
      return false;
    }
  }
  return true;
}

CachingStrategy MultiRecordFileReader::getCachingStrategy() const {
  if (!isOpened_) {
    return CachingStrategy::Passive;
  }
  return readers_.front()->getCachingStrategy();
}

bool MultiRecordFileReader::prefetchRecordSequence(
    const vector<const IndexRecord::RecordInfo*>& records,
    bool clearSequence) {
  if (!isOpened_) {
    return false;
  }
  // Split the input prefetch sequence into sequences correponding to each underlying Reader
  map<RecordFileReader*, vector<const IndexRecord::RecordInfo*>> readerPrefetchSequenceMap;
  for (const auto* prefetchRecord : records) {
    RecordFileReader* reader = getReader(prefetchRecord);
    if (reader == nullptr) {
      XR_LOGW("Illegal record provided to prefetchRecordSequence()");
      return false;
    }
    readerPrefetchSequenceMap[reader].emplace_back(prefetchRecord);
  }
  for (auto& [reader, prefetchSequence] : readerPrefetchSequenceMap) {
    if (!reader->prefetchRecordSequence(prefetchSequence, clearSequence)) {
      return false;
    }
  }
  return true;
}

bool MultiRecordFileReader::purgeFileCache() {
  if (!isOpened_) {
    return true;
  }
  bool succeeded = true;
  for (auto& reader : readers_) {
    succeeded = reader->purgeFileCache() && succeeded;
  }
  return succeeded;
}

const IndexRecord::RecordInfo* MultiRecordFileReader::getRecordByTime(double timestamp) const {
  if (!isOpened_) {
    return nullptr;
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecordByTime(timestamp);
  }
  const auto lowerBound =
      std::lower_bound(recordIndex_->cbegin(), recordIndex_->cend(), timestamp, timestampLT);
  return lowerBound == recordIndex_->cend() ? nullptr : *lowerBound;
}

const IndexRecord::RecordInfo* MultiRecordFileReader::getRecordByTime(
    UniqueStreamId streamId,
    double timestamp) const {
  if (!isOpened_) {
    return nullptr;
  }
  if (hasSingleFile()) {
    return readers_.front()->getRecordByTime(streamId, timestamp);
  }
  const StreamIdReaderPair* streamIdReaderPair = getStreamIdReaderPair(streamId);
  if (streamIdReaderPair == nullptr) {
    return nullptr;
  }
  const RecordFileReader* reader = streamIdReaderPair->second;
  return reader->getRecordByTime(streamIdReaderPair->first, timestamp);
}

const IndexRecord::RecordInfo* MultiRecordFileReader::getNearestRecordByTime(
    double timestamp,
    double epsilon,
    StreamId streamId) const {
  if (!isOpened_) {
    return nullptr;
  }
  if (hasSingleFile()) {
    return readers_.front()->getNearestRecordByTime(timestamp, epsilon, streamId);
  }
  if (streamId.isValid()) {
    const StreamIdReaderPair* streamIdReaderPair = getStreamIdReaderPair(streamId);
    if (streamIdReaderPair == nullptr) {
      return nullptr;
    }
    const RecordFileReader* reader = streamIdReaderPair->second;
    return reader->getNearestRecordByTime(timestamp, epsilon, streamIdReaderPair->first);
  }

  return vrs::getNearestRecordByTime(*recordIndex_, timestamp, epsilon);
}

std::unique_ptr<FileHandler> MultiRecordFileReader::getFileHandler() const {
  if (readers_.empty()) {
    return nullptr;
  }
  return readers_.front()->getFileHandler();
}

UniqueStreamId MultiRecordFileReader::getUniqueStreamId(
    const IndexRecord::RecordInfo* record) const {
  if (!isOpened_ || record == nullptr) {
    return {};
  }
  if (hasSingleFile()) {
    return record->streamId;
  }
  const RecordFileReader* reader = getReader(record);
  if (reader == nullptr) {
    return record->streamId;
  }
  return getUniqueStreamIdInternal(reader, record->streamId);
}

int64_t MultiRecordFileReader::getTotalSourceSize() const {
  int64_t totalSize = 0;
  for (const auto& reader : readers_) {
    totalSize += reader->getTotalSourceSize();
  }
  return totalSize;
}

bool MultiRecordFileReader::readFirstConfigurationRecord(
    UniqueStreamId uniqueStreamId,
    StreamPlayer* streamPlayer) {
  if (!isOpened_) {
    return false;
  }
  if (hasSingleFile()) {
    return readers_.front()->readFirstConfigurationRecord(uniqueStreamId, streamPlayer);
  }
  const StreamIdReaderPair* streamIdReaderPair = getStreamIdReaderPair(uniqueStreamId);
  if (streamIdReaderPair != nullptr) {
    RecordFileReader* reader = streamIdReaderPair->second;
    return reader->readFirstConfigurationRecord(streamIdReaderPair->first, streamPlayer);
  } else {
    return false;
  }
}

bool MultiRecordFileReader::readFirstConfigurationRecords(StreamPlayer* streamPlayer) {
  if (!isOpened_) {
    return false;
  }
  bool allGood = true;
  for (const auto& reader : readers_) {
    allGood = reader->readFirstConfigurationRecords(streamPlayer) && allGood;
  }
  return allGood;
}

bool MultiRecordFileReader::readFirstConfigurationRecordsForType(
    RecordableTypeId typeId,
    StreamPlayer* streamPlayer) {
  if (!isOpened_) {
    return false;
  }
  bool allGood = true;
  for (const auto& reader : readers_) {
    allGood = reader->readFirstConfigurationRecordsForType(typeId, streamPlayer) && allGood;
  }
  return allGood;
}

bool MultiRecordFileReader::areFilesRelated() const {
  if (readers_.empty() || hasSingleFile()) {
    return true;
  }
  for (const auto& relatedTag : kRelatedFileTags) {
    auto readerIt = readers_.cbegin();
    // The first non-empty value must be treated as the expectedValue
    string expectedValue;
    do {
      expectedValue = (*readerIt)->getTag(relatedTag);
      readerIt++;
    } while (expectedValue.empty() && readerIt != readers_.cend());
    if (expectedValue.empty()) {
      // This relatedTag is not present in any of the readers_ so we can move on to the next
      // relatedTag
      continue;
    }
    while (readerIt != readers_.cend()) {
      const auto actualValue = (*readerIt)->getTag(relatedTag);
      if (!actualValue.empty() && expectedValue != actualValue) {
        XR_LOGE(
            "Unrelated file found. Reader #: {}, Tag: '{}', ExpectedValue: '{}', ActualValue: '{}'",
            readerIt - readers_.cbegin(),
            relatedTag,
            expectedValue,
            actualValue);
        return false;
      }
      readerIt++;
    } // end while readerIt
  } // end for relatedTag
  return true;
}

void MultiRecordFileReader::createConsolidatedIndex() {
  if (hasSingleFile()) {
    // Memory optimization for single file case - leverage the RecordFileReader index directly.
    recordIndex_ = nullptr;
    return;
  }
  uint32_t indexSize = 0;
  // Holds RecordInfo* sorted in non-decreasing order of corresponding timestamps.
  // We will store only one element from each RecordFileReader index and perform a K-way merge.
  priority_queue<
      const IndexRecord::RecordInfo*,
      std::vector<const IndexRecord::RecordInfo*>,
      decltype(recordComparatorGT_)>
      recordPQueue(recordComparatorGT_);
  // Stores the last valid (terminal) RecordInfo* for each RecordFileReader index.
  set<const IndexRecord::RecordInfo*> terminalRecordPtrs;
  for (const auto& reader : readers_) {
    const auto& readerIndex = reader->getIndex();
    if (readerIndex.empty()) {
      continue;
    }
    recordPQueue.emplace(&readerIndex.front());
    terminalRecordPtrs.emplace(&readerIndex.back());
    indexSize += readerIndex.size();
  }
  recordIndex_ = make_unique<std::vector<const IndexRecord::RecordInfo*>>();
  recordIndex_->reserve(indexSize);
  while (!recordPQueue.empty()) {
    const auto record = recordPQueue.top();
    recordPQueue.pop();
    recordIndex_->push_back(record);
    if (terminalRecordPtrs.find(record) == terminalRecordPtrs.end()) {
      // If record is not terminal, add the next record to the priority queue
      recordPQueue.emplace(record + 1);
    }
  }
}

void MultiRecordFileReader::initializeFileTags() {
  for (const auto& reader : readers_) {
    const auto& fileTags = reader->getTags();
    fileTags_.insert(fileTags.begin(), fileTags.end());
  }
}

UniqueStreamId MultiRecordFileReader::generateUniqueStreamId(StreamId duplicateStreamId) const {
  auto candidateStreamId = duplicateStreamId;
  auto typeId = candidateStreamId.getTypeId();
  do {
    candidateStreamId = StreamId(typeId, candidateStreamId.getInstanceId() + 1);
  } while (uniqueStreamIds_.find(candidateStreamId) != uniqueStreamIds_.end());
  return candidateStreamId;
}

const MultiRecordFileReader::StreamIdReaderPair* MultiRecordFileReader::getStreamIdReaderPair(
    UniqueStreamId uniqueStreamId) const {
  const auto it = uniqueToStreamIdReaderPairMap_.find(uniqueStreamId);
  if (it == uniqueToStreamIdReaderPairMap_.end()) {
    return nullptr;
  }
  return &it->second;
}

const string& MultiRecordFileReader::getTag(const map<string, string>& tags, const string& name)
    const {
  auto iter = tags.find(name);
  if (iter != tags.end()) {
    return iter->second;
  }
  static const string sEmptyString;
  return sEmptyString;
}

void MultiRecordFileReader::initializeUniqueStreamIds() {
  if (hasSingleFile()) {
    // Optimization for single file use case - no need to handle any StreamId collisions
    return;
  }
  for (const auto& readerPtr : readers_) {
    for (const auto& streamId : readerPtr->getStreams()) {
      UniqueStreamId uniqueStreamId;
      if (uniqueStreamIds_.find(streamId) == uniqueStreamIds_.end()) {
        // Newly seen StreamId - UniqueStreamId can be same as StreamId
        uniqueStreamId = streamId;
      } else {
        // Colliding StreamId
        uniqueStreamId = generateUniqueStreamId(streamId);
      }
      readerStreamIdToUniqueMap_[readerPtr.get()][streamId] = uniqueStreamId;
      uniqueToStreamIdReaderPairMap_.emplace(
          uniqueStreamId, std::make_pair(streamId, readerPtr.get()));
      uniqueStreamIds_.emplace(uniqueStreamId);
    }
  }
}

RecordFileReader* MultiRecordFileReader::getReader(const IndexRecord::RecordInfo* record) const {
  for (const auto& reader : readers_) {
    if (belongsTo(record, *reader)) {
      return reader.get();
    }
  }
  return nullptr;
}

bool MultiRecordFileReader::timeLessThan(
    const IndexRecord::RecordInfo* lhs,
    const IndexRecord::RecordInfo* rhs) const {
  if (lhs->timestamp != rhs->timestamp) {
    return lhs->timestamp < rhs->timestamp;
  }
  // When timestamps are the same, we need to map the records to their `UniqueStreamId`, which we
  // can then compare. Fortunately, that should be rare.
  const auto uniqueStreamIdLhs = getUniqueStreamIdInternal(lhs);
  const auto uniqueStreamIdRhs = getUniqueStreamIdInternal(rhs);
  return uniqueStreamIdLhs < uniqueStreamIdRhs ||
      (uniqueStreamIdLhs == uniqueStreamIdRhs && lhs->fileOffset < rhs->fileOffset);
}

UniqueStreamId MultiRecordFileReader::getUniqueStreamIdInternal(
    const IndexRecord::RecordInfo* record) const {
  return getUniqueStreamIdInternal(getReader(record), record->streamId);
}

UniqueStreamId MultiRecordFileReader::getUniqueStreamIdInternal(
    const RecordFileReader* reader,
    StreamId streamId) const {
  return readerStreamIdToUniqueMap_.at(reader).at(streamId);
}

} // namespace vrs
