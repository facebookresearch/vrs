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

#include "RecordFileReader.h"

#include <algorithm>

#define DEFAULT_LOG_CHANNEL "RecordFileReader"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/CompilerAttributes.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

#include "DataLayout.h"
#include "DescriptionRecord.h"
#include "DiskFile.h"
#include "ErrorCode.h"
#include "FileCache.h"
#include "FileDetailsCache.h"
#include "FileHandlerFactory.h"
#include "IndexRecord.h"
#include "LegacyFormatsProvider.h"
#include "StreamPlayer.h"
#include "TagsRecord.h"
#include "TelemetryLogger.h"

using namespace std;

namespace vrs {

StreamPlayer::~StreamPlayer() = default;

RecordFileReader::RecordFileReader() {
  file_ = make_unique<DiskFile>();
}

RecordFileReader::~RecordFileReader() {
  closeFile();
  TelemetryLogger::flush();
}

void RecordFileReader::setOpenProgressLogger(ProgressLogger* progressLogger) {
  if (progressLogger != nullptr) {
    openProgressLogger_ = progressLogger;
  } else {
    // so we can always assume we have a logger active
    static SilentLogger sSilentLogger;
    openProgressLogger_ = &sSilentLogger;
  }
}

void RecordFileReader::setFileHandler(unique_ptr<FileHandler> fileHandler) {
  if (fileHandler) {
    file_ = std::move(fileHandler);
  }
}

unique_ptr<FileHandler> RecordFileReader::getFileHandler() const {
  return file_->makeNew();
}

void RecordFileReader::setStreamPlayer(StreamId streamId, StreamPlayer* streamPlayer) {
  streamPlayers_[streamId] = streamPlayer;
  if (streamPlayer != nullptr) {
    streamPlayer->onAttachedToFileReader(*this, streamId);
  }
}

int RecordFileReader::openFile(const FileSpec& fileSpec, bool autoWriteFixedIndex) {
  return doOpenFile(fileSpec, autoWriteFixedIndex, /* checkSignatureOnly */ false);
}

int RecordFileReader::openFile(const string& filePath, bool autoWriteFixedIndex) {
  FileSpec fileSpec;
  IF_ERROR_RETURN(vrsFilePathToFileSpec(filePath, fileSpec));
  return doOpenFile(fileSpec, autoWriteFixedIndex, /* checkSignatureOnly */ false);
}

int RecordFileReader::vrsFilePathToFileSpec(const string& filePath, FileSpec& outFileSpec) {
  IF_ERROR_RETURN(outFileSpec.fromPathJsonUri(filePath));
  if (!outFileSpec.isDiskFile()) {
    return SUCCESS;
  }
  if (outFileSpec.chunks.empty()) {
    XR_LOGW("Invalid path spec '{}'", filePath);
    return INVALID_FILE_SPEC;
  }
  outFileSpec.chunkSizes.clear();
  if (outFileSpec.chunks.size() > 1) {
    for (auto& chunk : outFileSpec.chunks) {
      if (!os::isFile(chunk)) {
        XR_LOGW("File '{}' not found", chunk);
        return DISKFILE_FILE_NOT_FOUND;
      }
    }
    return SUCCESS;
  }
  // if we have only one chunk, resolve the link (if it's a link), and look for further chunks
  // next to the target of the link, not next to the link itself...
  string& firstChunk = outFileSpec.chunks.front();
  string targetFile; // path to the file, or the linked file, if filePath was a link
  os::getLinkedTarget(firstChunk, targetFile);
  if (!os::isFile(targetFile)) {
    if (targetFile == firstChunk) {
      XR_LOGW("File '{}' not found", firstChunk);
    } else {
      XR_LOGW("Linked file '{}' from '{}' not found", targetFile, firstChunk);
    }
    return DISKFILE_FILE_NOT_FOUND;
  }
  firstChunk = targetFile;

  string root;
  if (helpers::endsWith(targetFile, "_0")) {
    // pattern: "anything_0" -> "anything_1", "anything_2", ...
    root.assign(targetFile, 0, targetFile.size() - 1); // strip trailing '0' char
  } else {
    // pattern: "anything" -> "anything_1", "anything_2", ...
    root = targetFile + '_';
  }
  for (size_t index = 1; /* nothing to check */; index++) {
    string chunkName = root + to_string(index);
    if (!os::isFile(chunkName)) {
      break; // it is not an error to not find more chunks, but we stop searching
    }
    outFileSpec.chunks.push_back(chunkName);
  }
  return SUCCESS;
}

bool RecordFileReader::isOpened() const {
  return file_->isOpened();
}

bool RecordFileReader::isVrsFile(const FileSpec& fileSpec) {
  return doOpenFile(fileSpec, /* autoWriteFixedIndex */ false, /* checkSignatureOnly */ true) == 0;
}

bool RecordFileReader::isVrsFile(const string& filePath) {
  FileSpec fileSpec;
  if (fileSpec.fromPathJsonUri(filePath) != 0) {
    return false;
  }
  return isVrsFile(fileSpec);
}

// Log progress & bail, if user cancelled
#define LOG_PROGRESS(operation__, error__, messageLamba__)           \
  do {                                                               \
    openProgressLogger_->setDetailedProgress(file_->showProgress()); \
    if (!openProgressLogger_->logNewStep(messageLamba__())) {        \
      closeFile();                                                   \
      XR_LOGW("Open cancelled");                                     \
      return OPERATION_CANCELLED;                                    \
    }                                                                \
    error__ = operation__;                                           \
  } while (false)

int RecordFileReader::doOpenFile(
    const FileSpec& fileSpec,
    bool autoWriteFixedIndex,
    bool checkSignatureOnly) {
  int error = 0;

  double beforeTime = os::getTimestampSec();
  // open file + read header + read description + read index record + read index
  const int kOpenTotalStepCount = 5;
  openProgressLogger_->setStepCount(kOpenTotalStepCount);
  LOG_PROGRESS(FileHandlerFactory::getInstance().delegateOpen(fileSpec, file_), error, [&]() {
    const string& fileHandlerName =
        fileSpec.fileHandlerName.empty() ? file_->getFileHandlerName() : fileSpec.fileHandlerName;
    return "Opening " + fileHandlerName + " file";
  });
  // log remote file handler names with success/failure status
  if (file_ && file_->isRemoteFileSystem()) {
    OperationContext context{"RecordFileReader::doOpenFile", file_->getFileHandlerName()};
    if (error != 0) {
      TelemetryLogger::error(context, errorCodeToMessageWithCode(error));
    } else {
      TelemetryLogger::info(context, "success");
    }
  }
  if (error != 0 || file_->getTotalSize() < static_cast<int64_t>(sizeof(FileFormat::FileHeader))) {
    if (error != 0) {
      XR_LOGE(
          "Could not open the file '{}': {}",
          fileSpec.getEasyPath(),
          errorCodeToMessageWithCode(error));
    } else {
      XR_LOGE(
          "File '{}' is too small to be a valid VRS file ({} bytes).",
          fileSpec.getEasyPath(),
          file_->getTotalSize());
      error = NOT_A_VRS_FILE;
    }
    if (!file_) {
      file_ = make_unique<DiskFile>();
    }
    return error;
  }
  TemporaryCachingStrategy temporaryCachingStrategy(*file_, CachingStrategy::Passive);
  FileFormat::FileHeader fileHeader;
  LOG_PROGRESS(readFileHeader(fileSpec, fileHeader), error, [&]() {
    string fileSize = helpers::humanReadableFileSize(file_->getTotalSize());
    return "Reading " + fileSize + ' ' + file_->getFileHandlerName() + " file header";
  });
  if (error != 0) {
    closeFile();
    XR_LOGE("Couldn't read file header: {}", errorCodeToMessageWithCode(error));
    return error;
  }
  recordHeaderSize_ = fileHeader.recordHeaderSize.get();
  if (!fileHeader.looksLikeAVRSFile()) {
    closeFile();
    if (!checkSignatureOnly) {
      XR_LOGE("File header integrity check failed: this doesn't look like a VRS file.");
    }
    return NOT_A_VRS_FILE;
  }
  if (!fileHeader.isFormatSupported()) {
    closeFile();
    XR_LOGE(
        "The file '{}' was created using a newer version of VRS, and can not be read.\n"
        "Please update your app to use the latest version of VRS.",
        fileSpec.getEasyPath());
    return UNSUPPORTED_VRS_FILE;
  }
  if (checkSignatureOnly) {
    closeFile();
    return 0;
  }
  string detailsCacheFilePath;
  FileCache* fileCache = FileCache::getFileCache();
  bool tryToUseCache = file_->isRemoteFileSystem() && fileHeader.creationId.get() != 0;
  if (!tryToUseCache || fileCache == nullptr ||
      fileCache->getFile(
          "vrs_details_" + to_string(fileHeader.creationId.get()) + '_' +
              to_string(file_->getTotalSize()),
          detailsCacheFilePath) != 0 ||
      FileDetailsCache::read(
          detailsCacheFilePath,
          streamIds_,
          fileTags_,
          streamTags_,
          recordIndex_,
          fileHasAnIndex_) != 0) {
    error = readFileDetails(fileSpec, autoWriteFixedIndex, fileHeader);
    // Maybe write the file's details to disk
    if (tryToUseCache && !detailsCacheFilePath.empty()) {
      detailsSaveThread_ = make_unique<thread>([detailsCacheFilePath, this]() {
        // It's safe to use those member variables, because we won't modify them until closeFile(),
        // which will join() the thread first.
        int writeStatus = FileDetailsCache::write(
            detailsCacheFilePath,
            streamIds_,
            fileTags_,
            streamTags_,
            recordIndex_,
            fileHasAnIndex_);
        if (writeStatus == 0) {
          XR_LOGI("File details written out to cache as '{}'", detailsCacheFilePath);
        } else {
          XR_LOGE(
              "Failed to write file details to '{}'. Error: {}",
              detailsCacheFilePath,
              errorCodeToMessage(writeStatus));
        }
      });
    }
  } else {
    openProgressLogger_->logNewStep("Read file details from cache");
  }
  openProgressLogger_->logDuration("File open", os::getTimestampSec() - beforeTime);
  endOfUserRecordsOffset_ = fileHeader.getEndOfUserRecordsOffset(file_->getTotalSize());
  return error;
}

int RecordFileReader::readFileHeader(
    const FileSpec& fileSpec,
    FileFormat::FileHeader& outFileHeader) {
  bool readHeaderFromCache = false;
  FileCache* fileCache = FileCache::getFileCache();
  string headerCacheFilePath;
  if (fileCache != nullptr && file_->isRemoteFileSystem()) {
    string fileName =
        "vrs_header_x" + fileSpec.getXXHash() + "_" + to_string(file_->getTotalSize());
    if (fileCache->getFile(fileName, headerCacheFilePath) == 0 &&
        DiskFile::readZstdFile(headerCacheFilePath, &outFileHeader, sizeof(outFileHeader)) == 0 &&
        outFileHeader.looksLikeAVRSFile()) {
      openProgressLogger_->logNewStep("Loaded header from cache");
      readHeaderFromCache = true;
    }
  }
  if (!readHeaderFromCache) {
    IF_ERROR_LOG_AND_RETURN(file_->read(outFileHeader));
    if (!headerCacheFilePath.empty()) {
      DiskFile::writeZstdFile(headerCacheFilePath, &outFileHeader, sizeof(outFileHeader));
    }
  }
  return 0;
}

int RecordFileReader::readFileDetails(
    const FileSpec& fileSpec,
    bool autoWriteFixedIndex,
    FileFormat::FileHeader& fileHeader) {
  int error = 0;
  int64_t firstUserRecordOffset = fileHeader.firstUserRecordOffset.get();
  if (firstUserRecordOffset == 0) {
    // firstUserRecordOffset was only created when we added support for early index records.
    firstUserRecordOffset = fileHeader.fileHeaderSize.get();
  }
  // Read the description record
  int64_t descriptionRecordOffset = fileHeader.descriptionRecordOffset.get();
  if (descriptionRecordOffset > 0) {
    if (file_->setPos(descriptionRecordOffset) == 0) {
      uint32_t descriptionSize = 0;
      LOG_PROGRESS(
          DescriptionRecord::readDescriptionRecord(
              *file_, fileHeader.recordHeaderSize.get(), descriptionSize, streamTags_, fileTags_),
          error,
          []() { return "Read description record"; });
      if (error != 0) {
        XR_LOGW("Error reading the file description record: {}", errorCodeToMessageWithCode(error));
      }
      // In early files, the first user record comes after the description record, if any.
      if (descriptionRecordOffset == firstUserRecordOffset) {
        firstUserRecordOffset += descriptionSize;
      }
    } else {
      XR_LOGW("Error accessing the file description record.");
    }
  } else {
    XR_LOGW("No description record.");
  }
  // Read the file's index
  file_->setCachingStrategy(CachingStrategy::Streaming);
  IndexRecord::Reader indexReader(
      *file_, fileHeader, openProgressLogger_, streamIds_, recordIndex_);
  int64_t usedFileSize = 0;
  LOG_PROGRESS(indexReader.readRecord(firstUserRecordOffset, usedFileSize), error, []() {
    return "Read index record";
  });
  if (error != 0) {
    XR_LOGW("Could not read index record: {}", errorCodeToMessageWithCode(error));
  }
  fileHasAnIndex_ = (error == 0 && indexReader.isIndexComplete());
  if (fileHasAnIndex_) {
    if (usedFileSize > 0) {
      file_->forgetFurtherChunks(usedFileSize);
    }
    if (autoWriteFixedIndex) {
      XR_LOGI("The file's index seems fine, so the file won't be modified.");
    }
  } else {
    if (file_->isRemoteFileSystem()) {
      TelemetryLogger::warning(
          {"RecordFileReader::open", fileSpec.getSourceLocation()}, "Index is incomplete.");
    }
    XR_LOGW("Index incomplete. Rebuilding index of '{}'...", fileSpec.getEasyPath());
    indexReader.rebuildIndex(autoWriteFixedIndex);
    if (!file_->isReadOnly()) {
      XR_LOGI("Re-opening file in read-only mode.");
      return doOpenFile(fileSpec, /* autoWriteFixedIndex */ false, /* checkSignatureOnly */ false);
    }
  }
  // Read all the tag records immediately
  unique_ptr<TagsRecordPlayer> tagsPlayer;
  for (auto iter = recordIndex_.begin();
       iter != recordIndex_.end() && iter->timestamp <= TagsRecord::kTagsRecordTimestamp;
       ++iter) {
    if (iter->recordType == Record::Type::TAGS) {
      if (!tagsPlayer) {
        tagsPlayer = make_unique<TagsRecordPlayer>(this, streamTags_);
      }
      XR_LOGD("Reading TagsRecord for {}", iter->streamId.getName());
      tagsPlayer->prepareToReadTagsFor(iter->streamId);
      readRecord(*iter, tagsPlayer.get());
    }
  }
  // If there was any, remove all the TagsRecords from the index.
  // No other code than this should ever "see" any TagsRecord when reading a file...
  if (tagsPlayer) {
    MAYBE_UNUSED size_t sizeBefore = recordIndex_.size();
    recordIndex_.erase(
        remove_if(
            recordIndex_.begin(),
            recordIndex_.end(),
            [](const IndexRecord::RecordInfo& record) {
              return record.recordType == Record::Type::TAGS;
            }),
        recordIndex_.end());
    XR_LOGD("Deleted {} TagsRecords from the index.", sizeBefore - recordIndex_.size());
    DescriptionRecord::createStreamSerialNumbers(fileTags_, streamTags_);
  }
  // Streams with no record won't be revealed by the index.
  for (auto& tags : streamTags_) {
    streamIds_.insert(tags.first);
  }
  return 0;
}

int RecordFileReader::closeFile() {
  int result = file_->close();
  if (detailsSaveThread_) {
    detailsSaveThread_->join();
    detailsSaveThread_.reset();
  }
  streamIds_.clear();
  streamTags_.clear();
  fileTags_.clear();
  recordIndex_.clear();
  openProgressLogger_ = &defaultProgressLogger_;
  streamIndex_.clear();
  lastRequest_.clear();
  fileHasAnIndex_ = false;
  return result;
}

int RecordFileReader::clearStreamPlayers() {
  if (file_->isOpened()) {
    return INVALID_REQUEST;
  }
  streamPlayers_.clear();
  return 0;
}

bool RecordFileReader::prefetchRecordSequence(
    const vector<const IndexRecord::RecordInfo*>& records,
    bool clearSequence) {
  if (!XR_VERIFY(endOfUserRecordsOffset_ > static_cast<int64_t>(recordHeaderSize_)) ||
      !file_->isRemoteFileSystem()) {
    return false; // don't even try for local file systems!
  }
  // records are not always perfectly sorted, so we can't tell easily where they end.
  // The best guess, is the offset of the first record, after the current record...
  // yep, that's a bit expensive, but we have few options...
  vector<int64_t> recordBoundaries;
  recordBoundaries.reserve(recordIndex_.size() + 1);
  int64_t lastOffset = 0;
  bool sortNeeded = false;
  for (const auto& r : recordIndex_) {
    recordBoundaries.emplace_back(r.fileOffset);
    if (r.fileOffset < lastOffset) {
      sortNeeded = true;
    }
    lastOffset = r.fileOffset;
  }
  int64_t fileSize = file_->getTotalSize();
  recordBoundaries.emplace_back(
      endOfUserRecordsOffset_ < fileSize ? endOfUserRecordsOffset_ : fileSize);
  if (sortNeeded || recordBoundaries.back() < lastOffset) {
    sort(recordBoundaries.begin(), recordBoundaries.end());
  }
  vector<pair<size_t, size_t>> segments;
  segments.reserve(records.size());
  for (const IndexRecord::RecordInfo* record : records) {
    int64_t recordOffset = record->fileOffset;
    if (XR_VERIFY(recordOffset < fileSize)) {
      auto nextBoundary =
          upper_bound(recordBoundaries.begin(), recordBoundaries.end(), recordOffset);
      if (XR_VERIFY(nextBoundary != recordBoundaries.end())) {
        int64_t nextRecordOffset = *nextBoundary;
        if (XR_VERIFY(nextRecordOffset > recordOffset)) {
          segments.emplace_back(
              static_cast<size_t>(recordOffset),
              static_cast<size_t>(nextRecordOffset - recordOffset - 1));
        }
      }
    }
  }
  return file_->prefetchReadSequence(segments, clearSequence);
}

bool RecordFileReader::hasIndex() const {
  return fileHasAnIndex_;
}

vector<StreamId> RecordFileReader::getStreams(RecordableTypeId typeId, const string& flavor) const {
  vector<StreamId> streamIds;
  for (const auto& streamId : streamIds_) {
    if ((typeId == RecordableTypeId::Undefined || streamId.getTypeId() == typeId) &&
        (flavor.empty() || getFlavor(streamId) == flavor)) {
      streamIds.emplace_back(streamId);
    }
  }
  return streamIds;
}

StreamId RecordFileReader::getStreamForType(RecordableTypeId typeId, uint32_t indexNumber) const {
  uint32_t hitCount = 0;
  for (const auto& streamId : streamIds_) {
    if (streamId.getTypeId() == typeId && hitCount++ == indexNumber) {
      return streamId;
    }
  }
  return StreamId{RecordableTypeId::Undefined, 0};
}

StreamId RecordFileReader::getStreamForFlavor(
    RecordableTypeId typeId,
    const string& flavor,
    uint32_t indexNumber) const {
  uint32_t hitCount = 0;
  for (const auto& streamId : streamIds_) {
    if (streamId.getTypeId() == typeId && getFlavor(streamId) == flavor &&
        hitCount++ == indexNumber) {
      return streamId;
    }
  }
  return {};
}

StreamId RecordFileReader::getStreamForTag(
    const string& tagName,
    const string& tag,
    RecordableTypeId typeId) const {
  for (const auto& streamId : streamIds_) {
    if ((typeId == RecordableTypeId::Undefined || streamId.getTypeId() == typeId) &&
        getTag(streamId, tagName) == tag) {
      return streamId;
    }
  }
  return {};
}

StreamId RecordFileReader::getStreamForSerialNumber(const std::string& streamSerialNumber) const {
  for (const auto& streamId : streamIds_) {
    if (getSerialNumber(streamId) == streamSerialNumber) {
      return streamId;
    }
  }
  return {};
}

const IndexRecord::RecordInfo* RecordFileReader::getRecord(uint32_t globalIndex) const {
  return globalIndex < recordIndex_.size() ? &recordIndex_[globalIndex] : nullptr;
}

const IndexRecord::RecordInfo* RecordFileReader::getRecord(StreamId streamId, uint32_t indexNumber)
    const {
  const auto& index = getIndex(streamId);
  return indexNumber < index.size() ? index[indexNumber] : nullptr;
}

const IndexRecord::RecordInfo* RecordFileReader::getRecord(
    StreamId streamId,
    Record::Type recordType,
    uint32_t indexNumber) const {
  const auto& index = getIndex(streamId);
  if (indexNumber >= index.size()) {
    return nullptr;
  }
  uint32_t hitCount = 0;
  size_t searchIndex = 0;
  // See if we searched for this streamId/recordType, to speed looking for a next index
  const pair<StreamId, Record::Type> queryType(streamId, recordType);
  auto lastRequest = lastRequest_.find(queryType);
  if (lastRequest != lastRequest_.end() && indexNumber >= lastRequest->second.first) {
    hitCount = lastRequest->second.first;
    searchIndex = lastRequest->second.second;
  }
  for (/* nothing */; searchIndex < index.size(); searchIndex++) {
    const IndexRecord::RecordInfo* record = index[searchIndex];
    if (record->recordType == recordType && hitCount++ == indexNumber) {
      // save this request's result, to speed-up sequential reads
      lastRequest_[queryType] = make_pair(indexNumber, searchIndex);
      return record;
    }
  }
  return nullptr;
}

const IndexRecord::RecordInfo* RecordFileReader::getLastRecord(
    StreamId streamId,
    Record::Type recordType) const {
  const auto& index = getIndex(streamId);
  for (auto iter = index.rbegin(); iter != index.rend(); ++iter) {
    if ((*iter)->recordType == recordType) {
      return *iter;
    }
  }
  return nullptr;
}

static bool timeCompare(const IndexRecord::RecordInfo& lhs, const IndexRecord::RecordInfo& rhs) {
  return lhs.timestamp < rhs.timestamp;
}

const IndexRecord::RecordInfo* RecordFileReader::getRecordByTime(double timestamp) const {
  IndexRecord::RecordInfo firstTime(timestamp, 0, StreamId(), Record::Type());
  auto lowerBound = lower_bound(recordIndex_.begin(), recordIndex_.end(), firstTime, timeCompare);
  if (lowerBound != recordIndex_.end()) {
    return &*lowerBound;
  }
  return nullptr;
}

const IndexRecord::RecordInfo* RecordFileReader::getRecordByTime(
    Record::Type recordType,
    double timestamp) const {
  IndexRecord::RecordInfo firstTime(timestamp, 0, StreamId(), Record::Type());
  auto lowerBound = lower_bound(recordIndex_.begin(), recordIndex_.end(), firstTime, timeCompare);
  while (lowerBound != recordIndex_.end() && lowerBound->recordType != recordType) {
    ++lowerBound;
  }
  if (lowerBound != recordIndex_.end()) {
    return &*lowerBound;
  }
  return nullptr;
}

static bool ptrTimeCompare(const IndexRecord::RecordInfo* lhs, const IndexRecord::RecordInfo* rhs) {
  return lhs->timestamp < rhs->timestamp;
}

const IndexRecord::RecordInfo* RecordFileReader::getRecordByTime(
    StreamId streamId,
    double timestamp) const {
  const vector<const IndexRecord::RecordInfo*>& index = getIndex(streamId);
  const IndexRecord::RecordInfo firstTime(timestamp, 0, StreamId(), Record::Type());

  auto lowerBound = lower_bound(index.begin(), index.end(), &firstTime, ptrTimeCompare);
  // The stream index is a vector of pointers in the recordIndex_ vector!
  if (lowerBound != index.end()) {
    return *lowerBound;
  }
  return nullptr;
}

const IndexRecord::RecordInfo* RecordFileReader::getRecordByTime(
    StreamId streamId,
    Record::Type recordType,
    double timestamp) const {
  const vector<const IndexRecord::RecordInfo*>& index = getIndex(streamId);
  const IndexRecord::RecordInfo firstTime(timestamp, 0, StreamId(), Record::Type());

  auto lowerBound = lower_bound(index.begin(), index.end(), &firstTime, ptrTimeCompare);
  while (lowerBound != index.end() && (*lowerBound)->recordType != recordType) {
    ++lowerBound;
  }
  // The stream index is a vector of pointers in the recordIndex_ vector!
  if (lowerBound != index.end()) {
    return *lowerBound;
  }
  return nullptr;
}

const IndexRecord::RecordInfo* RecordFileReader::getNearestRecordByTime(
    double timestamp,
    double epsilon,
    StreamId streamId,
    Record::Type recordType) const {
  // If stream id is undefined, we search for all the streams.
  if (streamId.isValid()) {
    const vector<const IndexRecord::RecordInfo*>& index = getIndex(streamId);
    return vrs::getNearestRecordByTime(index, timestamp, epsilon, recordType);
  }

  const IndexRecord::RecordInfo* nearest = nullptr;
  const IndexRecord::RecordInfo firstTime(timestamp, 0, StreamId(), Record::Type());
  auto lowerBound = lower_bound(recordIndex_.begin(), recordIndex_.end(), firstTime, timeCompare);

  auto start = (lowerBound == recordIndex_.begin()) ? lowerBound : lowerBound - 1;
  auto end = (lowerBound == recordIndex_.end()) ? lowerBound : lowerBound + 1;
  for (auto iter = start; iter != end; iter++) {
    double diff = std::abs((*iter).timestamp - timestamp);
    if (diff <= epsilon &&
        (nearest == nullptr || diff < std::abs(nearest->timestamp - timestamp)) &&
        (recordType == Record::Type::UNDEFINED || iter->recordType == recordType)) {
      nearest = &(*iter);
    }
  }

  return nearest;
}

uint32_t RecordFileReader::getRecordIndex(const IndexRecord::RecordInfo* record) const {
  if (!recordIndex_.empty() && record >= &recordIndex_.front() && record <= &recordIndex_.back()) {
    return static_cast<uint32_t>(record - &(recordIndex_.front()));
  }
  return static_cast<uint32_t>(recordIndex_.size());
}

uint32_t RecordFileReader::getRecordStreamIndex(const IndexRecord::RecordInfo* record) const {
  const vector<const IndexRecord::RecordInfo*>& index = getIndex(record->streamId);
  if (!index.empty() && record >= index.front() && record <= index.back()) {
    // Records are sorted in the global index, and a stream's index is a subset of the global index,
    // sorted as well even though they are pointers.
    // So we can search for the pointer directly in the stream's index by value using lower_bound.
    auto match = lower_bound(index.begin(), index.end(), record);
    if (match != index.end()) {
      return static_cast<uint32_t>(match - index.begin());
    }
  }
  // Return the size of the global index as an invalid value, because it's always greater than the
  // size of any of the indexes.
  return static_cast<uint32_t>(recordIndex_.size());
}

const vector<const IndexRecord::RecordInfo*>& RecordFileReader::getIndex(StreamId streamId) const {
  // recordableIndex_ is only initialized when we need it the first time. When we do,
  // we create an index for all the typess.
  if (streamIndex_.empty() && (!streamIds_.empty() && !recordIndex_.empty())) {
    // We need to create the indexes. First, calculate their size
    map<StreamId, uint32_t> recordCounter;
    for (const auto& recordIndex : recordIndex_) {
      recordCounter[recordIndex.streamId]++;
    }
    // Reserve space in the vectors, so that emplace_back never needs to re-allocate memory
    for (auto iter : recordCounter) {
      streamIndex_[iter.first].reserve(iter.second);
    }
    // We can now create the indexes, trusting that the emplace_back operations will be trivial
    for (const auto& recordIndex : recordIndex_) {
      streamIndex_[recordIndex.streamId].emplace_back(&recordIndex);
    }
  }
  return streamIndex_[streamId];
}

uint32_t RecordFileReader::getRecordCount(StreamId streamId) const {
  return static_cast<uint32_t>(getIndex(streamId).size());
}

uint32_t RecordFileReader::getRecordCount(StreamId streamId, Record::Type recordType) const {
  const auto& recordIndex = getIndex(streamId);
  uint32_t count = 0;
  for (const auto& recordInfo : recordIndex) {
    if (recordInfo->recordType == recordType) {
      count++;
    }
  }
  return count;
}

double RecordFileReader::getFirstDataRecordTime() const {
  for (const auto& record : recordIndex_) {
    if (record.recordType == Record::Type::DATA) {
      return record.timestamp;
    }
  }
  return 0; // no data record...
}

bool RecordFileReader::readFirstConfigurationRecord(StreamId streamId, StreamPlayer* streamPlayer) {
  const IndexRecord::RecordInfo* config = getRecord(streamId, Record::Type::CONFIGURATION, 0);
  if (config == nullptr) {
    return false;
  } else if (streamPlayer == nullptr) {
    return readRecord(*config) == 0;
  }
  streamPlayer->onAttachedToFileReader(*this, streamId);
  return readRecord(*config, streamPlayer) == 0;
}

bool RecordFileReader::readFirstConfigurationRecords(StreamPlayer* streamPlayer) {
  bool foundAtLeastOneStream = false;
  bool allGood = true;
  for (const auto& streamId : streamIds_) {
    foundAtLeastOneStream = true;
    allGood = readFirstConfigurationRecord(streamId, streamPlayer) && allGood;
  }
  return foundAtLeastOneStream && allGood;
}

bool RecordFileReader::readFirstConfigurationRecordsForType(
    RecordableTypeId typeId,
    StreamPlayer* streamPlayer) {
  bool foundAtLeastOneStream = false;
  bool allGood = true;
  for (const auto& streamId : streamIds_) {
    if (streamId.getTypeId() == typeId) {
      foundAtLeastOneStream = true;
      allGood = readFirstConfigurationRecord(streamId, streamPlayer) && allGood;
    }
  }
  return foundAtLeastOneStream && allGood;
}

bool RecordFileReader::getRecordFormat(
    StreamId streamId,
    Record::Type recordType,
    uint32_t formatVersion,
    RecordFormat& outFormat) const {
  string formatStr = getTag(
      getTags(streamId).vrs, RecordFormat::getRecordFormatTagName(recordType, formatVersion));
  if (formatStr.empty()) {
    outFormat = ContentType::CUSTOM;
    return false;
  }
  outFormat.set(formatStr);
  return true;
}

uint32_t RecordFileReader::getRecordFormats(StreamId streamId, RecordFormatMap& outFormats) const {
  outFormats.clear();
  RecordFormat::getRecordFormats(getTags(streamId).vrs, outFormats);
  RecordFormatRegistrar::getInstance().getLegacyRecordFormats(streamId.getTypeId(), outFormats);
  return static_cast<uint32_t>(outFormats.size());
}

unique_ptr<DataLayout> RecordFileReader::getDataLayout(
    StreamId streamId,
    const ContentBlockId& blockId) const {
  const map<string, string>& vrsTags = getTags(streamId).vrs;
  unique_ptr<DataLayout> layout = RecordFormat::getDataLayout(vrsTags, blockId);
  if (!layout) {
    layout = RecordFormatRegistrar::getInstance().getLegacyDataLayout(blockId);
  }
  return layout;
}

const string& RecordFileReader::getTag(const map<string, string>& tags, const string& name) const {
  auto iter = tags.find(name);
  if (iter != tags.end()) {
    return iter->second;
  }
  static const string sEmptyString;
  return sEmptyString;
}

const StreamTags& RecordFileReader::getTags(StreamId streamId) const {
  auto iter = streamTags_.find(streamId);
  if (iter != streamTags_.end()) {
    return iter->second;
  }
  static const StreamTags sEmptyRecordableTags;
  return sEmptyRecordableTags;
}

const string& RecordFileReader::getOriginalRecordableTypeName(StreamId streamId) const {
  return getTag(getTags(streamId).vrs, Recordable::getOriginalNameTagName());
}

const string& RecordFileReader::getFlavor(StreamId streamId) const {
  return getTag(getTags(streamId).vrs, Recordable::getFlavorTagName());
}

const string& RecordFileReader::getSerialNumber(StreamId streamId) const {
  return getTag(getTags(streamId).vrs, Recordable::getSerialNumberTagName());
}

bool RecordFileReader::mightContainImages(StreamId streamId) const {
  return mightContainContentTypeInDataRecord(streamId, ContentType::IMAGE);
}

bool RecordFileReader::mightContainAudio(StreamId streamId) const {
  return mightContainContentTypeInDataRecord(streamId, ContentType::AUDIO);
}

bool RecordFileReader::mightContainContentTypeInDataRecord(StreamId streamId, ContentType type)
    const {
  RecordFormatMap formats;
  if (getRecordFormats(streamId, formats) > 0) {
    for (const auto& format : formats) {
      if (format.second.getBlocksOfTypeCount(type) > 0) {
        // Find a data record for that stream, but don't create a stream index if none exists yet
        auto iter = streamIndex_.find(streamId);
        if (iter != streamIndex_.end()) {
          for (const IndexRecord::RecordInfo* record : iter->second) {
            if (record->recordType == Record::Type::DATA) {
              return true;
            }
          }
        } else {
          for (const IndexRecord::RecordInfo& record : recordIndex_) {
            if (record.streamId == streamId && record.recordType == Record::Type::DATA) {
              return true;
            }
          }
        }
        return false;
      }
    }
  }
  return false;
}

int RecordFileReader::readAllRecords() {
  if (!file_->isOpened()) {
    XR_LOGE("No file open");
    return NO_FILE_OPEN;
  }
  int error = 0;
  for (auto& recordInfo : recordIndex_) {
    if ((error = readRecord(recordInfo)) != 0) {
      break;
    }
  }
  return error;
}

vector<pair<string, int64_t>> RecordFileReader::getFileChunks() const {
  return file_->getFileChunks();
}

int64_t RecordFileReader::getTotalSourceSize() const {
  return file_->getTotalSize();
}

bool RecordFileReader::isRecordAvailableOrPrefetch(const IndexRecord::RecordInfo& recordInfo) {
  map<StreamId, StreamPlayer*>::iterator iter = streamPlayers_.find(recordInfo.streamId);
  StreamPlayer* streamPlayer = (iter == streamPlayers_.end()) ? nullptr : iter->second;
  if (!file_->isOpened()) {
    return false;
  }
  if (streamPlayer == nullptr) {
    return false;
  }
  IF_ERROR_LOG_AND_RETURN(file_->setPos(recordInfo.fileOffset));
  FileFormat::RecordHeader recordHeader;
  if (!file_->isAvailableOrPrefetch(sizeof(recordHeader))) {
    return false;
  }
  // Since the header is immediately available, we read it (cheap) to figure out how much other
  // data needs to already be in the cache to consider this record complete.
  int error = file_->read(recordHeader);
  if (error != 0) {
    XR_LOGE(
        "Record #{} Could not read record header: {}",
        getRecordIndex(&recordInfo),
        errorCodeToMessageWithCode(error));
    return false;
  }
  uint32_t recordSize = recordHeader.recordSize.get();
  return file_->isAvailableOrPrefetch(recordSize);
}

int RecordFileReader::readRecord(const IndexRecord::RecordInfo& recordInfo) {
  map<StreamId, StreamPlayer*>::iterator iter = streamPlayers_.find(recordInfo.streamId);
  StreamPlayer* streamPlayer = (iter == streamPlayers_.end()) ? nullptr : iter->second;
  return readRecord(recordInfo, streamPlayer);
}

int RecordFileReader::readRecord(
    const IndexRecord::RecordInfo& recordInfo,
    StreamPlayer* streamPlayer) {
  if (!file_->isOpened()) {
    XR_LOGE("No file open");
    return NO_FILE_OPEN;
  }
  // If there is no handler for this stream, we don't need to do anything, since all our readRecord
  // operations tell which record to read, and we always do an absolute seek first. We used to have
  // readNextRecord(), that relied on readRecord to get you at the beginning of the next record, but
  // since it's gone, we can save ourselves potentially quite a few fseek and fread operations.
  if (streamPlayer == nullptr) {
    return 0;
  }
  IF_ERROR_LOG_AND_RETURN(file_->setPos(recordInfo.fileOffset));
  if (recordHeaderSize_ < sizeof(FileFormat::RecordHeader)) {
    // this header is smaller than expected! Did the file format change?
    // Implement support for that case later, if necessary.
    XR_LOGE("Record #{} Record header too small", static_cast<int>(&recordInfo - &getIndex()[0]));
    return INVALID_DISK_DATA;
  }
  FileFormat::RecordHeader recordHeader;
  int error = file_->read(recordHeader);
  if (error != 0) {
    if (file_->getLastRWSize() == 0 && file_->isEof()) {
      return 0; // nothing read & end of file: we're good!
    }
    XR_LOGE(
        "Record #{} Could not read record header: {}",
        static_cast<int>(&recordInfo - &getIndex()[0]),
        errorCodeToMessageWithCode(error));
    return error;
  }
  uint32_t recordSize = recordHeader.recordSize.get();
  if (recordSize < recordHeaderSize_) {
    XR_LOGE(
        "Record #{} Record size too small. Expected: {} Actual: {}",
        static_cast<int>(&recordInfo - &getIndex()[0]),
        recordHeaderSize_,
        recordSize);
    // the record size is smaller than the record header size! corrupt file?...
    return INVALID_DISK_DATA;
  }

  bool integrityCheck = true;
  if (recordInfo.timestamp != recordHeader.timestamp.get()) {
    integrityCheck = false;
    XR_LOGE(
        "Record #{} Timestamp does not match. Expected: {} Actual: {}",
        static_cast<int>(&recordInfo - &getIndex()[0]),
        recordInfo.timestamp,
        recordHeader.timestamp.get());
  }

  if (recordInfo.recordType != recordHeader.getRecordType()) {
    integrityCheck = false;
    XR_LOGE(
        "Record #{} Record type does not match. Expected: {}/{} Actual: {}/{}",
        static_cast<int>(&recordInfo - &getIndex()[0]),
        toString(recordInfo.recordType),
        static_cast<int>(recordInfo.recordType),
        toString(recordInfo.recordType),
        static_cast<int>(recordHeader.getRecordType()));
  }

  if (recordInfo.streamId != recordHeader.getStreamId()) {
    integrityCheck = false;
    XR_LOGE(
        "Record #{} StreamId does not match. Expected: {} Actual: {}",
        static_cast<int>(&recordInfo - &getIndex()[0]),
        recordInfo.streamId.getName(),
        recordHeader.getStreamId().getName());
  }

  if (!integrityCheck) {
    return INVALID_DISK_DATA;
  }
  uint32_t dataSize = recordSize - recordHeaderSize_;
  uint32_t uncompressedDataSize;
  RecordReader* reader = nullptr;
  CompressionType compressionType = recordHeader.getCompressionType();
  switch (compressionType) {
    case CompressionType::None:
      uncompressedDataSize = dataSize;
      reader = uncompressedRecordReader_.init(*file_, dataSize, dataSize);
      break;
    case CompressionType::Lz4:
    case CompressionType::Zstd:
      uncompressedDataSize = recordHeader.uncompressedSize.get();
      reader = compressedRecordReader_.init(*file_, dataSize, uncompressedDataSize);
      compressedRecordReader_.initCompressionType(compressionType);
      break;
    default: // ignore the lint warning: the enum value was read from disk, so it could be anything!
      XR_LOGE(
          "Can't read record with unsupported compression in stream {}.\n"
          "You probably need a software update to read this file.",
          recordHeader.getStreamId().getName());
      return UNSUPPORTED_VRS_FILE;
  }
  CurrentRecord header{
      recordHeader.timestamp.get(),
      recordHeader.getStreamId(),
      recordHeader.getRecordType(),
      recordHeader.formatVersion.get(),
      uncompressedDataSize,
      reader,
      &recordInfo,
      this};
  DataReference dataReference;
  bool wantsData = reader != nullptr && streamPlayer->processRecordHeader(header, dataReference);
  uint32_t requestedSize = dataReference.getSize();
  if (wantsData && requestedSize <= uncompressedDataSize) {
    uint32_t readSize = 0;
    if (requestedSize > 0) {
      error = reader->read(dataReference, readSize);
      if (error != 0) {
        reader->finish();
        XR_LOGE("Read failed: {}", errorCodeToMessageWithCode(error));
        return error;
      }
    }
    streamPlayer->processRecord(header, readSize);
    reader->finish();
    return streamPlayer->recordReadComplete(*this, recordInfo);
  }
  return 0;
}

const IndexRecord::RecordInfo* getNearestRecordByTime(
    const vector<const IndexRecord::RecordInfo*>& index,
    double timestamp,
    double epsilon,
    Record::Type recordType) {
  const IndexRecord::RecordInfo* nearest = nullptr;
  const IndexRecord::RecordInfo firstTime(timestamp, 0, StreamId(), Record::Type());
  auto lowerBound = lower_bound(index.begin(), index.end(), &firstTime, ptrTimeCompare);

  auto start = (lowerBound == index.begin()) ? lowerBound : lowerBound - 1;
  auto end = (lowerBound == index.end()) ? lowerBound : lowerBound + 1;
  for (auto iter = start; iter != end; iter++) {
    double diff = std::abs((*iter)->timestamp - timestamp);
    if (diff <= epsilon &&
        (nearest == nullptr || diff < std::abs(nearest->timestamp - timestamp)) &&
        (recordType == Record::Type::UNDEFINED || (*iter)->recordType == recordType)) {
      nearest = *iter;
    }
  }
  return nearest;
}

} // namespace vrs
