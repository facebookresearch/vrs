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

#include "RecordFileWriter.h"

#include <algorithm>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#define DEFAULT_LOG_CHANNEL "RecordFileWriter"

#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/JobQueue.h>
#include <vrs/os/Event.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

#include "DescriptionRecord.h"
#include "ErrorCode.h"
#include "FileFormat.h"
#include "FileHandlerFactory.h"
#include "IndexRecord.h"
#include "Recordable.h"
#include "TagsRecord.h"

using namespace std;

namespace vrs {

#define LOG_FILE_OPERATIONS false

static double kMaxAutoCollectDelay = 10;
static double kDefaultAutoCollectDelay = 1;

namespace RecordFileWriter_ {

class CompressionJob {
 public:
  CompressionJob() = default;
  CompressionJob(const CompressionJob&) = delete;
  CompressionJob& operator=(const CompressionJob&) = delete;

  void setSortRecord(const RecordFileWriter::SortRecord& record) {
    sortRecord_ = record;
  }
  const RecordFileWriter::SortRecord& getSortRecord() const {
    return sortRecord_;
  }

  void performJob() {
    compressedSize_ = sortRecord_.record->compressRecord(compressor_);
  }

  uint32_t getCompressedSize() const {
    return compressedSize_;
  }
  Compressor& getCompressor() {
    return compressor_;
  }

 private:
  Compressor compressor_;
  RecordFileWriter::SortRecord sortRecord_{nullptr, {RecordableTypeId::Undefined, 0}};
  uint32_t compressedSize_{};
};

using CompressionJobQueue = JobQueue<CompressionJob*>;

class CompressionWorker {
 public:
  CompressionWorker(
      CompressionJobQueue& workQueue,
      CompressionJobQueue& resultsQueue,
      int threadIndex,
      InitCreatedThreadCallback initCreatedThreadCallback)
      : workQueue_{workQueue},
        resultsQueue_{resultsQueue},
        threadIndex_{threadIndex},
        initCreatedThreadCallback_{std::move(initCreatedThreadCallback)},
        thread_{&CompressionWorker::threadActivity, this} {}
  ~CompressionWorker() {
    thread_.join();
  }

 private:
  void threadActivity() {
    initCreatedThreadCallback_(thread_, ThreadRole::Compression, threadIndex_);

    CompressionJob* job = nullptr;
    while (workQueue_.waitForJob(job)) {
      job->performJob();
      resultsQueue_.sendJob(job);
    }
  }

  CompressionJobQueue& workQueue_;
  CompressionJobQueue& resultsQueue_;

  int threadIndex_;
  InitCreatedThreadCallback initCreatedThreadCallback_;

  thread thread_;
};

struct CompressionThreadsData {
  CompressionJobQueue jobsQueue;
  CompressionJobQueue resultsQueue;

  vector<unique_ptr<CompressionWorker>> compressionThreadsPool_;

  void addThreadUntil(
      size_t maxThreadPoolSize,
      const InitCreatedThreadCallback& initCreatedThreadCallback) {
    if (compressionThreadsPool_.size() < maxThreadPoolSize) {
      compressionThreadsPool_.reserve(maxThreadPoolSize);
      CompressionWorker* worker = new CompressionWorker(
          jobsQueue,
          resultsQueue,
          static_cast<int>(compressionThreadsPool_.size()),
          initCreatedThreadCallback);
      compressionThreadsPool_.emplace_back(worker);
    }
  }

  ~CompressionThreadsData() {
    jobsQueue.endQueue();
    compressionThreadsPool_.clear();
  }
};

// structure holding the background save thread's data, if any
struct WriterThreadData {
  WriterThreadData()
      : fileError{0},
        shouldEndThread{false},
        writeEventChannel{
            "WriterThreadDataWriteEventChannel",
            os::EventChannel::NotificationMode::UNICAST},
        hasRecordsReadyToWrite{false},
        maxTimestampProvider{nullptr},
        autoCollectDelay{0},
        nextAutoCollectTime{0} {
    // Do *not* start the thread here, as this will create race conditions.
    // The thread may start before the constructor even finished and returned, which means the
    // main thread may not even have a reference to this object.
  }

  ~WriterThreadData() {
    if (!shouldEndThread) {
      XR_LOGE("Unrequested exit of WriterThreadData");
    }
  }

  atomic<int> fileError; // 0, or last write error
  atomic<bool> shouldEndThread; // set when this thread should finish-up & end
  os::EventChannel writeEventChannel; // wake the background thread to write prepared records

  recursive_mutex mutex; // mutex to protect access to the following fields
  RecordFileWriter::RecordBatches recordsReadyToWrite;
  atomic<bool> hasRecordsReadyToWrite;
  function<double()> maxTimestampProvider; // for auto-writing records
  atomic<double> autoCollectDelay; // for auto-writing records
  double nextAutoCollectTime{}; // for auto-writing records

  CompressionThreadsData compressionThreadsData_;

  // not protected by mutex
  thread saveThread; // background thread to save records

  // set the file error, but only if there was none yet
  void setFileError(int error) {
    if (error != 0 && fileError == 0) {
      XR_LOGE("Error writing records: {}, {}", error, errorCodeToMessage(error));
      fileError = error;
    }
  }

  double getBackgroundThreadWaitTime();
};

struct PurgeThreadData {
  PurgeThreadData(
      const function<double()>& maxTimestampProvider,
      double autoPurgeDelay,
      bool purgePaused)
      : shouldEndThread{false},
        maxTimestampProvider{maxTimestampProvider},
        autoPurgeDelay{autoPurgeDelay},
        purgingPaused_{purgePaused} {}

  ~PurgeThreadData() {
    if (!shouldEndThread) {
      XR_LOGE("Unrequested exit of PurgeThreadData");
    }
  }

  atomic<bool> shouldEndThread; // set when the thread should end
  // os::EventChannel used to wake the purge background thread to purge old records
  os::EventChannel purgeEventChannel{
      "PurgeEventChannel",
      os::EventChannel::NotificationMode::UNICAST};

  recursive_mutex mutex; // mutex to protect access to the following fields
  function<double()> maxTimestampProvider; // for purging records, protected by mutex
  double autoPurgeDelay; // for purging records records, protected by mutex
  atomic<bool> purgingPaused_;

  // not protected by mutex
  thread purgeThread; // background thread to purge records
};

double WriterThreadData::getBackgroundThreadWaitTime() {
  double waitDelay = autoCollectDelay.load(memory_order_relaxed);
  if (waitDelay != 0.) {
    if (nextAutoCollectTime != 0.) {
      waitDelay = nextAutoCollectTime - os::getTimestampSec();
    }
    if (waitDelay < 0) {
      if (waitDelay < -1) {
        XR_LOGW_EVERY_N_SEC(
            5,
            "Compressing and saving the recording is {:.3f} seconds behind capturing the data, "
            "consider changing recording scope, destination, or compression settings.",
            -waitDelay);
      }
      waitDelay = 0;
    } else if (waitDelay > kMaxAutoCollectDelay) {
      waitDelay = kMaxAutoCollectDelay;
    }
  } else {
    waitDelay = kDefaultAutoCollectDelay;
  }
  return waitDelay;
}

static void logBatch(RecordFileWriter::RecordBatch& batch, const char* functionName) {
  size_t streamCount = 0;
  size_t recordCount = 0;
  for (const auto& r : batch) {
    streamCount++;
    recordCount += r.second.size();
  }
  XR_LOGD("{} {} records from {} streams.", functionName, recordCount, streamCount);
}

} // namespace RecordFileWriter_

using namespace vrs::RecordFileWriter_;

RecordFileWriter::RecordFileWriter()
    : file_{make_unique<DiskFile>()},
      indexRecordWriter_{fileHeader_},
      writerThreadData_{nullptr},
      purgeThreadData_{nullptr} {
  setMaxChunkSizeMB(0);
  initCreatedThreadCallback_ = [](thread&, ThreadRole, int) {};
#if IS_VRS_FB_INTERNAL() && LOG_FILE_OPERATIONS
  arvr::logging::getChannel(DEFAULT_LOG_CHANNEL).setLevel(arvr::logging::Level::Debug);
#endif
}

void RecordFileWriter::addRecordable(Recordable* recordable) {
  { // mutex guard
    lock_guard<mutex> lock(recordablesMutex_);
    for (auto* r : recordables_) {
      if (r != recordable && !XR_VERIFY(r->getStreamId() != recordable->getStreamId())) {
        return;
      }
    }
    recordables_.insert(recordable);
  } // mutex guard
  if (isWriting()) {
    // The file has been created already, so we must create a TagsRecord for the recordable's tags.
    TagsRecord tagsRecord;
    const StreamTags& tags = recordable->getStreamTags();
    tagsRecord.userTags.stage(tags.user);
    tagsRecord.vrsTags.stage(tags.vrs);
    recordable->createRecord(
        TagsRecord::kTagsRecordTimestamp,
        Record::Type::TAGS,
        TagsRecord::kTagsVersion,
        DataSource(tagsRecord));
    XR_LOGI(
        "Recordable {} is added after the file creation, so we're creating a TagsRecord "
        "for {} VRS tags and {} user tags.",
        recordable->getStreamId().getName(),
        tags.vrs.size(),
        tags.user.size());
    recordable->createConfigurationRecord();
    recordable->createStateRecord();
  }
}

vector<Recordable*> RecordFileWriter::getRecordables() const {
  vector<Recordable*> recordables;
  { // mutex guard
    lock_guard<mutex> lock(recordablesMutex_);
    recordables.reserve(recordables_.size());
    for (auto* recordable : recordables_) {
      recordables.push_back(recordable);
    }
  } // mutex guard
  return recordables;
}

void RecordFileWriter::setCompressionThreadPoolSize(size_t size) {
  compressionThreadPoolSize_ = min<size_t>(size, thread::hardware_concurrency());
}

void RecordFileWriter::setInitCreatedThreadCallback(
    const InitCreatedThreadCallback& initCreatedThreadCallback) {
  initCreatedThreadCallback_ = initCreatedThreadCallback;
}

int RecordFileWriter::writeToFile(const string& filePath) {
  if (isWriting()) {
    return FILE_ALREADY_OPEN;
  }
  // collect all the records
  RecordBatches recordBatches;
  recordBatches.emplace_back(new RecordBatch());
  collectOldRecords(*recordBatches.back(), Record::kMaxTimestamp);
  // assemble all the records in one multimap sorted by timestamp
  SortedRecords allRecords;
  addRecordBatchesToSortedRecords(recordBatches, allRecords);
  preliminaryIndex_ = make_unique<deque<IndexRecord::DiskRecordInfo>>();
  for (const auto& r : allRecords) {
    preliminaryIndex_->push_back({r.streamId, r.record});
  }
  int error = createFile(filePath, false);
  if (error != 0) {
    return error;
  }
  error = writeRecords(allRecords, error);
  // in case of error, don't bother writing an index or a description... :-(
  if (error != 0) {
    file_->close();
    return error;
  }
  return completeAndCloseFile();
}

void RecordFileWriter::purgeOldRecords(double maxTimestamp, bool recycleBuffers) {
  uint32_t total = 0;
  for (auto* recordable : getRecordables()) {
    total += recordable->getRecordManager().purgeOldRecords(maxTimestamp, recycleBuffers);
  }
  if (total > 0) {
    XR_LOGD("Purged {} old records.", total);
  }
}

void RecordFileWriter::backgroundWriterThreadActivity() {
  initCreatedThreadCallback_(writerThreadData_->saveThread, ThreadRole::Writer, 0);

  while (!writerThreadData_->shouldEndThread) {
    double waitDelay = writerThreadData_->getBackgroundThreadWaitTime();
    os::EventChannel::Event event{};
    os::EventChannel::Status status =
        writerThreadData_->writeEventChannel.waitForEvent(event, waitDelay);
    if (status == os::EventChannel::Status::SUCCESS) {
      if (!writerThreadData_->shouldEndThread) {
        backgroundWriteCollectedRecord();
      }
    } else if (status == os::EventChannel::Status::TIMEOUT) {
      if (autoCollectRecords(false)) {
        backgroundWriteCollectedRecord();
      }
    } else {
      XR_LOGE("Background thread quit on error");
      return;
    }
  }
  backgroundWriteCollectedRecord();
  if (writerThreadData_->fileError == 0) {
    writerThreadData_->setFileError(completeAndCloseFile());
  } else {
    auto fileError = writerThreadData_->fileError.load(std::memory_order_relaxed);
    XR_LOGW("Closed file with error #{}, {}", fileError, errorCodeToMessage(fileError));
    file_->close();
  }
  if (queueByteSize_) {
    queueByteSize_->store(0, memory_order_relaxed);
  }
  // resume purging records, if we were doing that
  if (purgeThreadData_ != nullptr) {
    purgeThreadData_->purgingPaused_ = false;
    purgeThreadData_->purgeEventChannel.dispatchEvent();
  }
  if (LOG_FILE_OPERATIONS) {
    XR_LOGD("Background thread ended.");
  }
}

bool RecordFileWriter::autoCollectRecords(bool checkTime) {
  bool somethingToWrite = false;

  const double now = os::getTimestampSec();
  if (checkTime && now < writerThreadData_->nextAutoCollectTime) {
    return false;
  }

  if (!writerThreadData_->shouldEndThread &&
      writerThreadData_->autoCollectDelay.load(memory_order_relaxed) != 0.) {
    { // mutex guard
      unique_lock<recursive_mutex> guard{writerThreadData_->mutex};
      double autoCollectDelay = writerThreadData_->autoCollectDelay.load(memory_order_relaxed);
      if (autoCollectDelay != 0. && writerThreadData_->maxTimestampProvider != nullptr) {
        writerThreadData_->nextAutoCollectTime = now + autoCollectDelay;
        unique_ptr<RecordBatch> newBatch = make_unique<RecordBatch>();
        if (collectOldRecords(*newBatch, writerThreadData_->maxTimestampProvider()) > 0) {
          if (LOG_FILE_OPERATIONS) {
            logBatch(*newBatch, "autoCollectRecords");
          }
          writerThreadData_->recordsReadyToWrite.emplace_back(std::move(newBatch));
          writerThreadData_->hasRecordsReadyToWrite.store(true, memory_order_relaxed);
          somethingToWrite = true;
        }
      }
    } // mutex guard
  }

  return somethingToWrite;
}

void RecordFileWriter::backgroundPurgeThreadActivity() {
  initCreatedThreadCallback_(purgeThreadData_->purgeThread, ThreadRole::Purge, 0);

  os::EventChannel::Status status = os::EventChannel::Status::SUCCESS;
  while (!purgeThreadData_->shouldEndThread &&
         (status == os::EventChannel::Status::SUCCESS ||
          status == os::EventChannel::Status::TIMEOUT)) {
    double waitDelay = 0;
    if (purgeThreadData_->purgingPaused_ || purgeThreadData_->autoPurgeDelay <= 0) {
      waitDelay = 1;
    } else {
      double maxTimestamp = numeric_limits<double>::lowest();
      { // mutex guard
        unique_lock<recursive_mutex> guard{purgeThreadData_->mutex};
        if (purgeThreadData_->maxTimestampProvider != nullptr) {
          maxTimestamp = purgeThreadData_->maxTimestampProvider();
        }
        waitDelay = purgeThreadData_->autoPurgeDelay;
      } // mutex guard
      if (waitDelay > 0 && maxTimestamp > numeric_limits<double>::lowest()) {
        purgeOldRecords(maxTimestamp);
      }
    }
    os::EventChannel::Event event{};
    status = purgeThreadData_->purgeEventChannel.waitForEvent(event, waitDelay);
  }
  if (status != os::EventChannel::Status::SUCCESS && status != os::EventChannel::Status::TIMEOUT) {
    XR_LOGE("Background thread quit on error");
  }
}

int RecordFileWriter::createFileAsync(const string& filePath, bool splitHead) {
  if (writerThreadData_ != nullptr) {
    return FILE_ALREADY_OPEN; // only one thread allowed!
  }
  int error = createFile(filePath, splitHead);
  if (error != 0) {
    indexRecordWriter_.reset();
    file_->close();
    return error;
  }
  if (LOG_FILE_OPERATIONS) {
    XR_LOGD("Created file {}", filePath);
  }
  if (purgeThreadData_ != nullptr) {
    purgeThreadData_->purgingPaused_ = true;
  }
  if (queueByteSize_) {
    queueByteSize_->store(0, memory_order_relaxed);
  }
  // make sure we have recent configuration & state records
  for (auto* recordable : getRecordables()) {
    recordable->createConfigurationRecord();
    recordable->createStateRecord();
  }
  writerThreadData_ = new WriterThreadData(); // Does not yet start the background thread.
  // Now it is safe to start the background thread.
  writerThreadData_->saveThread = thread{&RecordFileWriter::backgroundWriterThreadActivity, this};
  return 0;
}

int RecordFileWriter::createFileAsync(const string& filePath) {
  return createFileAsync(filePath, false);
}

int RecordFileWriter::createChunkedFile(
    const string& filePath,
    size_t maxChunkSizeMB,
    unique_ptr<NewChunkHandler>&& chunkHandler) {
  setMaxChunkSizeMB(maxChunkSizeMB);
  newChunkHandler_ = std::move(chunkHandler);
  return createFileAsync(filePath, true);
}

void RecordFileWriter::setMaxChunkSizeMB(size_t maxChunkSizeMB) {
  const size_t kMB = 1024 * 1024;
  const uint64_t kMaxFileSize = static_cast<uint64_t>(numeric_limits<int64_t>::max());
  if (maxChunkSizeMB == 0 || static_cast<uint64_t>(maxChunkSizeMB) >= kMaxFileSize / kMB) {
    maxChunkSize_ = kMaxFileSize; // 0 means max chunk size
  } else {
    maxChunkSize_ = maxChunkSizeMB * kMB;
  }
}

int RecordFileWriter::preallocateIndex(
    unique_ptr<deque<IndexRecord::DiskRecordInfo>> preliminaryIndex) {
  if (isWriting()) {
    return FILE_ALREADY_OPEN; // too late!
  }
  preliminaryIndex_ = std::move(preliminaryIndex);
  return 0;
}

int RecordFileWriter::writeRecordsAsync(double maxTimestamp) {
  if (writerThreadData_ == nullptr || writerThreadData_->shouldEndThread) {
    return INVALID_REQUEST;
  }

  unique_ptr<RecordBatch> recordBatch = make_unique<RecordBatch>();
  if (collectOldRecords(*recordBatch, maxTimestamp) > 0) {
    if (LOG_FILE_OPERATIONS) {
      logBatch(*recordBatch, "writeRecordsAsync");
    }
    { // mutex guard
      unique_lock<recursive_mutex> guard{writerThreadData_->mutex};
      writerThreadData_->recordsReadyToWrite.emplace_back(std::move(recordBatch));
      writerThreadData_->hasRecordsReadyToWrite.store(true, memory_order_relaxed);
    } // mutex guard
    writerThreadData_->writeEventChannel.dispatchEvent();
  }
  return writerThreadData_->fileError; // if there was some error already, say so
}

int RecordFileWriter::autoWriteRecordsAsync(
    const function<double()>& maxTimestampProvider,
    double delay) {
  if (writerThreadData_ == nullptr || writerThreadData_->shouldEndThread) {
    return INVALID_REQUEST;
  }
  { // mutex guard
    unique_lock<recursive_mutex> guard{writerThreadData_->mutex};
    writerThreadData_->maxTimestampProvider = maxTimestampProvider;
    writerThreadData_->autoCollectDelay.store(delay, memory_order_relaxed);
  } // mutex guard
  writeRecordsAsync(maxTimestampProvider());
  return 0;
}

int RecordFileWriter::autoPurgeRecords(
    const function<double()>& maxTimestampProvider,
    double delay) {
  if (purgeThreadData_ != nullptr) {
    unique_lock<recursive_mutex> guard{purgeThreadData_->mutex};
    purgeThreadData_->maxTimestampProvider = maxTimestampProvider;
    purgeThreadData_->autoPurgeDelay = delay;
    purgeThreadData_->purgeEventChannel.dispatchEvent();
  } else {
    purgeThreadData_ = new PurgeThreadData(
        maxTimestampProvider,
        delay,
        writerThreadData_ &&
            !writerThreadData_->shouldEndThread); // we are saving in the background
    // only start the thread once purgeThreadData_ has been set (race condition on start)
    purgeThreadData_->purgeThread = thread{&RecordFileWriter::backgroundPurgeThreadActivity, this};
  }
  return 0;
}

void RecordFileWriter::trackBackgroundThreadQueueByteSize() {
  if (queueByteSize_ == nullptr) {
    queueByteSize_ = make_unique<atomic<uint64_t>>();
  }
}

uint64_t RecordFileWriter::getBackgroundThreadQueueByteSize() {
  return queueByteSize_ ? queueByteSize_->load(memory_order_relaxed) : 0;
}

int RecordFileWriter::closeFileAsync() {
  if (writerThreadData_ == nullptr) {
    return NO_FILE_OPEN;
  }
  if (!writerThreadData_->shouldEndThread) {
    if (LOG_FILE_OPERATIONS) {
      XR_LOGD("File close request received.");
    }
    for (auto* recordable : getRecordables()) {
      recordable->getRecordManager().purgeCache();
    }
    writeRecordsAsync(Record::kMaxTimestamp);
    writerThreadData_->shouldEndThread = true;
    writerThreadData_->writeEventChannel.dispatchEvent();
  }
  return writerThreadData_->fileError; // if there was some error already, say so
}

int RecordFileWriter::waitForFileClosed() {
  if (writerThreadData_ == nullptr) {
    return NO_FILE_OPEN;
  }
  closeFileAsync();
  writerThreadData_->saveThread.join();
  int chunkError = 0;
  if (newChunkHandler_ != nullptr) {
    newChunkHandler_.reset();
  }
  // free all record memory
  for (auto* recordable : getRecordables()) {
    recordable->getRecordManager().purgeCache();
  }
  int diskError = writerThreadData_->fileError;
  delete writerThreadData_;
  writerThreadData_ = nullptr;
  return diskError != 0 ? diskError : chunkError;
}

void RecordFileWriter::setTag(const string& tagName, const string& tagValue) {
  // If we've already started to write the file already, we're too late: the description record is
  // already written and the file's tags can't be changed. This design is meant to prevent brittle
  // files designs, in which VRS files might be missing possibly critical information.
  if (isWriting()) {
    XR_LOGE("File tag added after file creation: it won't be written!");
  } else {
    fileTags_[tagName] = tagValue;
  }
}

void RecordFileWriter::addTags(const map<string, string>& newTags) {
  // If we've already started to write the file already, we're too late: the description record is
  // already written and the file's tags can't be changed. This design is meant to prevent brittle
  // files designs, in which VRS files might be missing possibly critical information.
  if (isWriting()) {
    XR_LOGE("File tags added after file creation: they won't be written!");
  } else {
    for (const auto& tag : newTags) {
      fileTags_[tag.first] = tag.second;
    }
  }
}

int RecordFileWriter::setWriteFileHandler(unique_ptr<WriteFileHandler> writeFileHandler) {
  if (isWriting()) {
    return FILE_ALREADY_OPEN;
  }
  file_ = std::move(writeFileHandler);
  return SUCCESS;
}

void RecordFileWriter::backgroundWriteCollectedRecord() {
  RecordFileWriter::SortedRecords recordsToWrite;
  if (addRecordsReadyToWrite(recordsToWrite)) {
    int error = writeRecords(recordsToWrite, writerThreadData_->fileError);
    writerThreadData_->setFileError(error);
  }
}

bool RecordFileWriter::addRecordsReadyToWrite(
    RecordFileWriter::SortedRecords& inOutRecordsToWrite) {
  if (!writerThreadData_->hasRecordsReadyToWrite.load(memory_order_relaxed)) {
    return false;
  }
  RecordBatches batches;
  { // mutex guard
    unique_lock<recursive_mutex> guard{writerThreadData_->mutex};
    batches.swap(writerThreadData_->recordsReadyToWrite);
    writerThreadData_->hasRecordsReadyToWrite.store(false, memory_order_relaxed);
  } // mutex guard
  uint64_t addedSize = addRecordBatchesToSortedRecords(batches, inOutRecordsToWrite);
  if (queueByteSize_) {
    queueByteSize_->fetch_add(addedSize, memory_order_relaxed);
  }
  return true;
}

uint64_t RecordFileWriter::collectOldRecords(RecordBatch& batch, double maxTimestamp) {
  uint64_t count = 0;
  auto recordables = getRecordables();
  batch.reserve(batch.size() + recordables.size());
  for (auto* recordable : recordables) {
    if (recordable->isRecordableActive()) {
      StreamId id = recordable->getStreamId();
      indexRecordWriter_.addStream(id);
      batch.push_back({id, {}});
      list<Record*>& oldRecords = batch.back().second;
      recordable->getRecordManager().collectOldRecords(maxTimestamp, oldRecords);
      count += oldRecords.size();
    } else {
      recordable->getRecordManager().purgeOldRecords(maxTimestamp);
    }
  }
  return count;
}

/// \brief Helper class pointing to the next record in a device's list of records.
/// This class doesn't not modify the list, simply iterating on it.
class RecordList {
 public:
  explicit RecordList(const pair<StreamId, list<Record*>>& deviceRecords)
      : deviceRecords_{&deviceRecords}, iter_{deviceRecords.second.begin()} {}
  RecordList(const RecordList& rhs) = default;
  RecordList& operator=(const RecordList& rhs) = default;

  // Get the record currently at the front of the
  inline RecordFileWriter::SortRecord getRecord() const {
    return {*iter_, deviceRecords_->first};
  }
  bool operator<(const RecordList& rhs) const {
    return rhs.getRecord() < getRecord(); // smallest element first
  }
  bool next() {
    return ++iter_ != deviceRecords_->second.end();
  }
  bool hasRecord() const {
    return iter_ != deviceRecords_->second.end();
  }

 private:
  const pair<StreamId, list<Record*>>* deviceRecords_;
  list<Record*>::const_iterator iter_; // pointer to the next record to consider
};

uint64_t RecordFileWriter::addRecordBatchesToSortedRecords(
    const RecordBatches& batches,
    SortedRecords& inOutSortedRecords) {
  uint64_t addedRecordSize = 0;
#if 0 // let's keep this reference (but slow) implementation for now
  for (const unique_ptr<RecordBatch>& batch : batches) {
    for (const pair<StreamId, list<Record*>>& idRecordList : *batch) {
      StreamId id = idRecordList.first;
      for (Record* record : idRecordList.second) {
        inOutSortedRecords.emplace_back(record, id);
        addedRecordSize += record->getSize();
      }
    }
  }
  sort(inOutSortedRecords.begin(), inOutSortedRecords.end());
#else
  // Priority queue to find which list has the next (oldest) record to add to the list.
  // Works best when the lists are sorted, but it's not an absolute requirement.
  priority_queue<RecordList, deque<RecordList>> pq;
  for (const unique_ptr<RecordBatch>& batch : batches) {
    for (const pair<StreamId, list<Record*>>& idRecordList : *batch) {
      if (!idRecordList.second.empty()) {
        pq.emplace(idRecordList);
      }
    }
  }
  while (!pq.empty()) {
    RecordList recordList = pq.top(); // get record list with the "oldest" record in front
    pq.pop();
    do {
      SortRecord rec = recordList.getRecord();
      // fast track the 99% case, because new batches come after previous batches
      if (inOutSortedRecords.empty() || inOutSortedRecords.back() < rec) {
        inOutSortedRecords.emplace_back(rec);
      } else {
        auto insertPoint = lower_bound(inOutSortedRecords.begin(), inOutSortedRecords.end(), rec);
        inOutSortedRecords.insert(insertPoint, rec);
      }
      addedRecordSize += rec.record->getSize();
    } while (recordList.next() && (pq.empty() || recordList.getRecord() < pq.top().getRecord()));
    if (recordList.hasRecord()) {
      pq.push(recordList);
    }
  }
#endif
  return addedRecordSize;
}

#define IF_ERROR_LOG_CLOSE_AND_RETURN(operation_)                                                  \
  {                                                                                                \
    int operationError_ = operation_;                                                              \
    if (operationError_ != 0) {                                                                    \
      XR_LOGE(                                                                                     \
          "{} failed: {}, {}", #operation_, operationError_, errorCodeToMessage(operationError_)); \
      head.close();                                                                                \
      return operationError_;                                                                      \
    }                                                                                              \
  }

int RecordFileWriter::createFile(const string& filePath, bool splitHead) {
  indexRecordWriter_.reset();
  FileSpec spec;
  int error = spec.fromPathJsonUri(filePath);
  if (error != 0) {
    XR_LOGW("Failed to parse path:  {}", filePath);
    return error;
  }

  if (!spec.isDiskFile()) {
    unique_ptr<WriteFileHandler> writeFile{dynamic_cast<WriteFileHandler*>(
        FileHandlerFactory::getInstance().getFileHandler(spec.fileHandlerName).release())};
    if (!writeFile) {
      XR_LOGE("Found no WriteFileHandler named {}.", spec.fileHandlerName);
      return INVALID_FILE_SPEC;
    }
    if (!writeFile->reopenForUpdatesSupported()) {
      // If a custom FileHandler can't handle updates, the file needs a local file where to write
      // the file's header, the description record, and index record, because that part needs to be
      // updated during file creation.
      // The rest of the file, which contains all the data records, can be written forward in one
      // pass, as it is being generated (no edits needed). In the upload case, the file's head will
      // need be uploaded and prepended to the uploaded data, after the file is closed.
      splitHead = true;
    }
    file_ = std::move(writeFile);
  } else if (spec.chunks.size() != 1) {
    XR_LOGE("File creation using {} requires a single file chunk.", spec.fileHandlerName);
    return INVALID_FILE_SPEC;
  }

  WriteFileHandler& head = splitHead ? indexRecordWriter_.initSplitHead() : *file_;
  error = head.create(spec.chunks.front());
  if (error != 0) {
    if (!splitHead && filePath == spec.chunks.front()) {
      XR_LOGE("Failed to create '{}': {}, {}", filePath, error, errorCodeToMessage(error));
    } else {
      XR_LOGE(
          "Failed to create {}'{}' at '{}': {}, {}",
          splitHead ? "the split head for " : "",
          filePath,
          spec.chunks.front(),
          error,
          errorCodeToMessage(error));
    }
    return error;
  }
  fileHeader_.init();
  fileHeader_.descriptionRecordOffset.set(sizeof(fileHeader_));
  IF_ERROR_LOG_CLOSE_AND_RETURN(head.write(fileHeader_))
  map<StreamId, const StreamTags*> streamTags;
  for (auto* recordable : getRecordables()) {
    streamTags[recordable->getStreamId()] = &(recordable->getStreamTags());
    indexRecordWriter_.addStream(recordable->getStreamId());
  }
  lastRecordSize_ = 0;
  IF_ERROR_LOG_CLOSE_AND_RETURN(
      DescriptionRecord::writeDescriptionRecord(head, streamTags, fileTags_, lastRecordSize_))

  if (splitHead) {
    IF_ERROR_LOG_CLOSE_AND_RETURN(indexRecordWriter_.createSplitIndexRecord(lastRecordSize_))
    IF_ERROR_LOG_CLOSE_AND_RETURN(file_->createSplitFile(spec, filePath));
  } else if (preliminaryIndex_ && !preliminaryIndex_->empty()) {
    // only use this preliminary index once
    unique_ptr<deque<IndexRecord::DiskRecordInfo>> index = std::move(preliminaryIndex_);
    IF_ERROR_LOG_CLOSE_AND_RETURN(
        indexRecordWriter_.preallocateClassicIndexRecord(head, *index, lastRecordSize_))
  } else {
    indexRecordWriter_.useClassicIndexRecord();
  }

  return 0;
}

int RecordFileWriter::writeRecords(SortedRecords& records, int lastError) {
  if (compressionThreadPoolSize_ == 0) {
    return writeRecordsSingleThread(records, lastError);
  }
  if (writerThreadData_ != nullptr) {
    return writeRecordsMultiThread(writerThreadData_->compressionThreadsData_, records, lastError);
  }
  CompressionThreadsData compressionThreadsData;
  return writeRecordsMultiThread(compressionThreadsData, records, lastError);
}

struct RecordFileWriter_::RecordWriterData {
  uint64_t currentChunkSize;
  int error;
  double oldest = numeric_limits<double>::max();
  double newest = numeric_limits<double>::lowest();
  uint64_t writtenRecords = 0;
  uint64_t skippedRecords = 0;
  uint64_t compressedRecords = 0;

  RecordWriterData(WriteFileHandler& fileHandler, int lastError)
      : currentChunkSize{static_cast<uint64_t>(fileHandler.getChunkPos())}, error{lastError} {}

  int getError() const {
    return error;
  }
  void logStat(uint64_t recordsToWriteCount, size_t compressionThreadCount) {
    // If an error occurred, it was already logged. Log success & (otherwise) silent skipping of
    // write operations...
    if (writtenRecords > 0) {
      if (writtenRecords == recordsToWriteCount) {
        XR_LOGD(
            "Wrote all {} records, compressed {} using {} threads, from {} to {}",
            writtenRecords,
            compressedRecords,
            compressionThreadCount,
            oldest,
            newest);
      } else {
        XR_LOGW(
            "Wrote {} out of {} records, compressed {} using {} threads, from {} to {}",
            writtenRecords,
            recordsToWriteCount,
            compressedRecords,
            compressionThreadCount,
            oldest,
            newest);
      }
    }
    if (skippedRecords > 0) {
      if (skippedRecords == recordsToWriteCount) {
        XR_LOGW("Skipped all {} records, from {} to {}", skippedRecords, oldest, newest);
      } else {
        XR_LOGW(
            "Skipped {} out of {} records, from {} to {}",
            skippedRecords,
            recordsToWriteCount,
            oldest,
            newest);
      }
    }
  }
  void logStat(uint64_t recordsToWriteCount) {
    // If an error occurred, it was already logged. Log success & (otherwise) silent skipping of
    // write operations...
    if (writtenRecords > 0) {
      if (writtenRecords == recordsToWriteCount) {
        XR_LOGD(
            "Wrote all {} records, compressed {}, from {} to {}",
            writtenRecords,
            compressedRecords,
            oldest,
            newest);
      } else {
        XR_LOGW(
            "Wrote {} out of {} records, compressed {}, from {} to {}",
            writtenRecords,
            recordsToWriteCount,
            compressedRecords,
            oldest,
            newest);
      }
    }
    if (skippedRecords > 0) {
      if (skippedRecords == recordsToWriteCount) {
        XR_LOGW("Skipped all {} records, from {} to {}", skippedRecords, oldest, newest);
      } else {
        XR_LOGW(
            "Skipped {} out of {} records, from {} to {}",
            skippedRecords,
            recordsToWriteCount,
            oldest,
            newest);
      }
    }
  }
};

void RecordFileWriter::writeOneRecord(
    RecordFileWriter_::RecordWriterData& rwd,
    Record* record,
    StreamId streamId,
    Compressor& compressor,
    uint32_t compressedSize) {
  double timestamp = record->getTimestamp();
  if (timestamp < rwd.oldest) {
    rwd.oldest = timestamp;
  }
  if (timestamp > rwd.newest) {
    rwd.newest = timestamp;
  }
  if (rwd.currentChunkSize > 0 && rwd.currentChunkSize + record->getSize() >= maxChunkSize_) {
    NewChunkNotifier newChunkNotifier(*file_, newChunkHandler_);
    // AddChunk() preserves the current chunk on error.
    XR_VERIFY(
        file_->addChunk() == 0,
        "Add chunk failed: {}, {}",
        file_->getLastError(),
        errorCodeToMessage(file_->getLastError()));
    rwd.currentChunkSize = 0; // reset, even if addChunk() failed to reduce retrying.
    newChunkNotifier.notify(1); // offset chunk index by 1
  }
  if (queueByteSize_) {
    queueByteSize_->fetch_sub(record->getSize(), memory_order_relaxed);
  }
  int error = record->writeRecord(*file_, streamId, lastRecordSize_, compressor, compressedSize);
  if (error != 0) {
    XR_LOGE("Write failed: {}, {}", error, errorCodeToMessage(error));
    rwd.error = error;
  } else {
    if (!skipFinalizeIndexRecords_) {
      indexRecordWriter_.addRecord(
          record->getTimestamp(), lastRecordSize_, streamId, record->getRecordType());
    }
    rwd.writtenRecords++;
    rwd.currentChunkSize += lastRecordSize_;
  }
  record->recycle();
}

int RecordFileWriter::writeRecordsSingleThread(SortedRecords& records, int lastError) {
  if (LOG_FILE_OPERATIONS) {
    XR_LOGD("Starting to write {} records", records.size());
  }
  RecordWriterData rwd(*file_, lastError);
  Compressor compressor;

  for (auto& r : records) {
    Record* record = r.record;
    if (rwd.getError() != 0) {
      rwd.skippedRecords++;
      record->recycle();
    } else {
      uint32_t compressedSize = record->compressRecord(compressor);
      if (compressedSize > 0) {
        rwd.compressedRecords++;
      }
      writeOneRecord(rwd, record, r.streamId, compressor, compressedSize);
    }
  }
  if (LOG_FILE_OPERATIONS) {
    rwd.logStat(records.size());
  }
  records.clear();
  return rwd.error;
}

int RecordFileWriter::writeRecordsMultiThread(
    CompressionThreadsData& compressionThreadsData,
    SortedRecords& recordsToCompress,
    int lastError) {
  uint64_t recordsToWriteCount = recordsToCompress.size();
  CompressionJob noCompressionJob;
#if IS_ANDROID_PLATFORM() || IS_IOS_PLATFORM()
  // Mobile platform have much tighter memory restrictions
  vector<CompressionJob> jobs(compressionThreadPoolSize_ * 4);
#else
  vector<CompressionJob> jobs(compressionThreadPoolSize_ * 20);
#endif
  vector<CompressionJob*> availableJobs;
  availableJobs.reserve(jobs.size());
  for (auto& job : jobs) {
    availableJobs.push_back(&job);
  }
  deque<SortRecord> writeQueue; // we need to preserve the order when writing data out!
  map<SortRecord, CompressionJob*> compressionResults;
  RecordWriterData rwd(*file_, lastError);

  while (!recordsToCompress.empty() || !writeQueue.empty() || !compressionResults.empty()) {
    double waitTime = 10;
    // See if we can find a new compressor for that job
    while (!recordsToCompress.empty() && !availableJobs.empty()) {
      SortRecord nextRecord = *recordsToCompress.begin();
      recordsToCompress.erase(recordsToCompress.begin());
      writeQueue.push_back(nextRecord);
      if (rwd.getError() == 0 && nextRecord.record->shouldTryToCompress()) {
        compressionThreadsData.addThreadUntil(
            compressionThreadPoolSize_, initCreatedThreadCallback_);
        // an idle worker might not have any job available (they haven't been written yet)
        CompressionJob* job = availableJobs.back();
        availableJobs.pop_back();
        job->setSortRecord(nextRecord);
        compressionThreadsData.jobsQueue.sendJob(job);
        rwd.compressedRecords++;
      } else {
        compressionResults.emplace(nextRecord, &noCompressionJob);
      }
      waitTime = 0;
    }
    map<SortRecord, CompressionJob*>::iterator resultsIter;
    // process completed compression job
    if (!writeQueue.empty() &&
        (resultsIter = compressionResults.find(writeQueue.front())) != compressionResults.end()) {
      Record* record = resultsIter->first.record;
      CompressionJob* job = resultsIter->second;
      if (rwd.getError() != 0) {
        rwd.skippedRecords++;
        record->recycle();
      } else {
        const StreamId streamId = resultsIter->first.streamId;
        writeOneRecord(rwd, record, streamId, job->getCompressor(), job->getCompressedSize());
      }
      if (job != &noCompressionJob) {
        job->getCompressor().clear();
        availableJobs.push_back(job);
      }
      compressionResults.erase(resultsIter);
      writeQueue.pop_front();
      waitTime = 0;
    }
    // Check if we have a results to process
    CompressionJob* job = nullptr;
    while (compressionThreadsData.resultsQueue.waitForJob(job, waitTime)) {
      compressionResults.emplace(job->getSortRecord(), job);
      waitTime = 0;
    }
    // Grab any new record ready to write, to feed our background threads ASAP
    autoCollectRecords(true);
    size_t previousCount = recordsToCompress.size();
    if (addRecordsReadyToWrite(recordsToCompress)) {
      recordsToWriteCount += recordsToCompress.size() - previousCount;
    }
  }

  if (LOG_FILE_OPERATIONS) {
    size_t compressionThreadCount = compressionThreadsData.compressionThreadsPool_.size();
    rwd.logStat(recordsToWriteCount, compressionThreadCount);
  }
  return rwd.error;
}

int RecordFileWriter::completeAndCloseFile() {
  if (!isWriting()) {
    return NO_FILE_OPEN;
  }
  int error = 0;
  if (!skipFinalizeIndexRecords_) {
    if (indexRecordWriter_.getSplitHead()) {
      error = indexRecordWriter_.finalizeSplitIndexRecord(newChunkHandler_);
    } else {
      int64_t endOfRecordsOffset = file_->getPos();
      if (endOfRecordsOffset >= 0) {
        error = indexRecordWriter_.finalizeClassicIndexRecord(
            *file_, endOfRecordsOffset, lastRecordSize_);
      } else {
        error = os::getLastFileError();
        XR_LOGE("Unable to get a file position to write an index!");
      }
    }
  }
  NewChunkNotifier newChunkNotifier(*file_, newChunkHandler_);
  int closeError = file_->close();
  error = (error != 0) ? error : closeError;
  if (error != 0) {
    XR_LOGW("File closed with error #{}, {}", error, errorCodeToMessage(error));
  } else if (LOG_FILE_OPERATIONS) {
    XR_LOGD("File closed, no error.");
  }
  newChunkNotifier.notify(1, true); // offset chunk index by 1, last chunk
  indexRecordWriter_.reset();
  return error;
}

RecordFileWriter::~RecordFileWriter() {
  if (writerThreadData_ != nullptr) {
    RecordFileWriter::waitForFileClosed(); // overrides not available in constructors & destructors
    delete writerThreadData_;
  }
  if (purgeThreadData_ != nullptr) {
    purgeThreadData_->shouldEndThread = true;
    purgeThreadData_->purgeEventChannel.dispatchEvent();
    purgeThreadData_->purgeThread.join();
    delete purgeThreadData_;
  }
}

} // namespace vrs
