// Facebook Technologies, LLC Proprietary and Confidential.

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

int MultiRecordFileReader::openFiles(const std::vector<std::string>& paths) {
  if (paths.empty()) {
    XR_LOGE("At least one file must be opened");
    return INVALID_REQUEST;
  }
  if (!readers_.empty()) {
    XR_LOGE("openFiles() must be invoked only once per instance");
    return INVALID_REQUEST;
  }
  readers_.reserve(paths.size());
  for (const auto& path : paths) {
    readers_.push_back(make_unique<RecordFileReader>());
    auto& reader = readers_.back();
    FileSpec fileSpec;
    IF_ERROR_RETURN(RecordFileReader::vrsFilePathToFileSpec(path, fileSpec));
    const auto status = reader->openFile(fileSpec);
    if (status != SUCCESS) {
      closeFiles();
      return status;
    }
    filePaths_.push_back(fileSpec.getEasyPath());
    XR_LOGD("Opened file '{}' and assigned to reader #{}", path, readers_.size() - 1);
  }
  if (!areFilesRelated()) {
    closeFiles();
    return INVALID_REQUEST;
  }
  createConsolidatedIndex();
  initializeUniqueStreamIds();
  isOpened_ = true;
  return SUCCESS;
}

int MultiRecordFileReader::closeFiles() {
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
  closeFiles();
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
  auto comparator = [](const IndexRecord::RecordInfo* a, const IndexRecord::RecordInfo* b) {
    return a->timestamp > b->timestamp;
  };
  // Holds RecordInfo* sorted in non-decreasing order of corresponding timestamps.
  // We will store only one element from each RecordFileReader index and perform a K-way merge.
  priority_queue<
      const IndexRecord::RecordInfo*,
      std::vector<const IndexRecord::RecordInfo*>,
      decltype(comparator)>
      recordPQueue(comparator);
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

} // namespace vrs
