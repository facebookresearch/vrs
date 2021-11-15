// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "RecordFileWriter.h"

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

#define LOG_FILE_OPERATIONS true

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
        initCreatedThreadCallback_{initCreatedThreadCallback},
        thread_(&CompressionWorker::threadActivity, this) {}
  ~CompressionWorker() {
    thread_.join();
  }

 private:
  void threadActivity() {
    initCreatedThreadCallback_(thread_, ThreadRole::Compression, threadIndex_);

    while (!workQueue_.hasEnded()) {
      CompressionJob* job;
      if (workQueue_.waitForJob(job, 1 /* second */)) {
        job->performJob();
        resultsQueue_.sendJob(job);
      }
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
      InitCreatedThreadCallback initCreatedThreadCallback) {
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
        autoCollectDelay{0} {
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

  std::recursive_mutex mutex; // mutex to protect access to the following fields
  RecordFileWriter::RecordBatches recordsReadyToWrite;
  std::atomic<bool> hasRecordsReadyToWrite;
  function<double()> maxTimestampProvider; // for auto-writing records
  std::atomic<double> autoCollectDelay; // for auto-writing records

  CompressionThreadsData compressionThreadsData_;

  // not protected by mutex
  thread saveThread; // background thread to save records

  // set the file error, but only if there was none yet
  void setFileError(int error) {
    if (error && !fileError) {
      XR_LOGE("Error writing records: {}, {}", error, errorCodeToMessage(error));
      fileError = error;
    }
  }

  double getBackgroundThreadWaitTime(const double& nextAutoCollectTime);
};

struct PurgeThreadData {
  PurgeThreadData(function<double()> maxTimestampProvider, double autoPurgeDelay, bool purgePaused)
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

  std::recursive_mutex mutex; // mutex to protect access to the following fields
  function<double()> maxTimestampProvider; // for purging records, protected by mutex
  double autoPurgeDelay; // for purging records records, protected by mutex
  atomic<bool> purgingPaused_;

  // not protected by mutex
  thread purgeThread; // background thread to purge records
};

double WriterThreadData::getBackgroundThreadWaitTime(const double& nextAutoCollectTime) {
  double waitDelay;
  if (autoCollectDelay.load(std::memory_order_relaxed) != 0.) {
    if (nextAutoCollectTime == 0.) {
      { // mutex guard
        std::unique_lock<std::recursive_mutex> guard{mutex};
        waitDelay = autoCollectDelay.load(std::memory_order_relaxed);
      } // mutex guard
    } else {
      waitDelay = nextAutoCollectTime - os::getTimestampSec();
    }
    if (waitDelay < 0) {
      waitDelay = 0;
    } else if (kMaxAutoCollectDelay > kMaxAutoCollectDelay) {
      waitDelay = kMaxAutoCollectDelay;
    }
  } else {
    waitDelay = kDefaultAutoCollectDelay;
  }
  return waitDelay;
}

} // namespace RecordFileWriter_

using namespace vrs::RecordFileWriter_;

RecordFileWriter::RecordFileWriter()
    : file_{make_unique<DiskFile>()},
      indexRecordWriter_{fileHeader_},
      writerThreadData_{nullptr},
      purgeThreadData_{nullptr} {
  setMaxChunkSizeMB(0);
  initCreatedThreadCallback_ = [](std::thread&, ThreadRole, int) {};
}

void RecordFileWriter::addRecordable(Recordable* recordable) {
  { // mutex guard
    std::lock_guard<std::mutex> lock(recordablesMutex_);
    for (auto r : recordables_) {
      if (r != recordable && !XR_VERIFY(r->getStreamId() != recordable->getStreamId())) {
        return;
      }
    }
    recordables_.insert(recordable);
  } // mutex guard
  if (isWriting()) {
    // The file has been created already, so we must create a TagsRecord for the recordable's tags.
    TagsRecord tagsRecord;
    const StreamTags& tags = recordable->getRecordableTags();
    tagsRecord.userTags.stage(tags.user);
    tagsRecord.vrsTags.stage(tags.vrs);
    recordable->createRecord(
        TagsRecord::kTagsRecordTimestamp,
        Record::Type::TAGS,
        TagsRecord::kTagsVersion,
        DataSource(tagsRecord));
    XR_LOGD(
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
    std::lock_guard<std::mutex> lock(recordablesMutex_);
    recordables.reserve(recordables_.size());
    for (auto recordable : recordables_) {
      recordables.push_back(recordable);
    }
  } // mutex guard
  return recordables;
}

void RecordFileWriter::setCompressionThreadPoolSize(size_t size) {
  if (size == kMaxThreadPoolSizeForHW) {
    size = thread::hardware_concurrency();
  }
  compressionThreadPoolSize_ = size;
}

void RecordFileWriter::setInitCreatedThreadCallback(
    InitCreatedThreadCallback initCreatedThreadCallback) {
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
  for (auto recordable : getRecordables()) {
    total += recordable->getRecordManager().purgeOldRecords(maxTimestamp, recycleBuffers);
  }
  if (total > 0) {
    XR_LOGD("Purged {} old records.", total);
  }
}

void RecordFileWriter::backgroundWriterThreadActivity() {
  initCreatedThreadCallback_(writerThreadData_->saveThread, ThreadRole::Writer, 0);

  double nextAutoCollectTime = 0;
  while (!writerThreadData_->shouldEndThread) {
    double waitDelay = writerThreadData_->getBackgroundThreadWaitTime(nextAutoCollectTime);
    os::EventChannel::Event event;
    os::EventChannel::Status status =
        writerThreadData_->writeEventChannel.waitForEvent(event, waitDelay);
    if (status == os::EventChannel::Status::SUCCESS) {
      if (!writerThreadData_->shouldEndThread) {
        backgroundWriteCollectedRecord();
      }
    } else if (status == os::EventChannel::Status::TIMEOUT) {
      if (writerThreadData_->autoCollectDelay.load(std::memory_order_relaxed) != 0.) {
        bool somethingToWrite = false;
        { // mutex guard
          std::unique_lock<std::recursive_mutex> guard{writerThreadData_->mutex};
          double autoCollectDelay =
              writerThreadData_->autoCollectDelay.load(std::memory_order_relaxed);
          if (autoCollectDelay != 0. && writerThreadData_->maxTimestampProvider != nullptr) {
            nextAutoCollectTime = os::getTimestampSec() + autoCollectDelay;
            unique_ptr<RecordBatch> newBatch = make_unique<RecordBatch>();
            if (collectOldRecords(*newBatch, writerThreadData_->maxTimestampProvider()) > 0) {
              writerThreadData_->recordsReadyToWrite.emplace_back(move(newBatch));
              writerThreadData_->hasRecordsReadyToWrite.store(true, std::memory_order_relaxed);
              somethingToWrite = true;
            }
          }
        } // mutex guard
        if (somethingToWrite) {
          backgroundWriteCollectedRecord();
        }
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
    XR_LOGW(
        "Closed file with error #{}, {}",
        writerThreadData_->fileError,
        errorCodeToMessage(writerThreadData_->fileError));
    file_->close();
  }
  // resume purging records, if we were doing that
  if (purgeThreadData_) {
    purgeThreadData_->purgingPaused_ = false;
    purgeThreadData_->purgeEventChannel.dispatchEvent();
  }
  if (LOG_FILE_OPERATIONS) {
    XR_LOGD("Background thread ended.");
  }
}

void RecordFileWriter::backgroundPurgeThreadActivity() {
  initCreatedThreadCallback_(purgeThreadData_->purgeThread, ThreadRole::Purge, 0);

  os::EventChannel::Status status = os::EventChannel::Status::SUCCESS;
  while (!purgeThreadData_->shouldEndThread &&
         (status == os::EventChannel::Status::SUCCESS ||
          status == os::EventChannel::Status::TIMEOUT)) {
    double waitDelay;
    if (purgeThreadData_->purgingPaused_ || purgeThreadData_->autoPurgeDelay <= 0) {
      waitDelay = 1;
    } else {
      double maxTimestamp = numeric_limits<double>::lowest();
      { // mutex guard
        std::unique_lock<std::recursive_mutex> guard{purgeThreadData_->mutex};
        if (purgeThreadData_->maxTimestampProvider != nullptr) {
          maxTimestamp = purgeThreadData_->maxTimestampProvider();
        }
        waitDelay = purgeThreadData_->autoPurgeDelay;
      } // mutex guard
      if (waitDelay > 0 && maxTimestamp > numeric_limits<double>::lowest()) {
        purgeOldRecords(maxTimestamp);
      }
    }
    os::EventChannel::Event event;
    status = purgeThreadData_->purgeEventChannel.waitForEvent(event, waitDelay);
  }
  if (status != os::EventChannel::Status::SUCCESS && status != os::EventChannel::Status::TIMEOUT) {
    XR_LOGE("Background thread quit on error");
  }
}

int RecordFileWriter::createFileAsync(const string& filePath, bool splitHead) {
  if (writerThreadData_) {
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
  if (purgeThreadData_) {
    purgeThreadData_->purgingPaused_ = true;
  }
  // make sure we have recent configuration & state records
  for (auto recordable : getRecordables()) {
    recordable->createConfigurationRecord();
    recordable->createStateRecord();
  }
  writerThreadData_ = new WriterThreadData(); // Does not yet start the background thread.
  // Now it is safe to start the background thread.
  writerThreadData_->saveThread =
      std::thread{&RecordFileWriter::backgroundWriterThreadActivity, this};
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
  const uint64_t kMaxFileSize = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
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
  preliminaryIndex_ = move(preliminaryIndex);
  return 0;
}

int RecordFileWriter::writeRecordsAsync(double maxTimestamp) {
  if (!writerThreadData_ || writerThreadData_->shouldEndThread) {
    return INVALID_REQUEST;
  }

  unique_ptr<RecordBatch> recordBatch = make_unique<RecordBatch>();
  if (collectOldRecords(*recordBatch, maxTimestamp) > 0) {
    { // mutex guard
      std::unique_lock<std::recursive_mutex> guard{writerThreadData_->mutex};
      writerThreadData_->recordsReadyToWrite.emplace_back(move(recordBatch));
      writerThreadData_->hasRecordsReadyToWrite.store(true, std::memory_order_relaxed);
    } // mutex guard
    writerThreadData_->writeEventChannel.dispatchEvent();
  }
  return writerThreadData_->fileError; // if there was some error already, say so
}

int RecordFileWriter::autoWriteRecordsAsync(function<double()> maxTimestampProvider, double delay) {
  if (!writerThreadData_ || writerThreadData_->shouldEndThread) {
    return INVALID_REQUEST;
  }
  { // mutex guard
    std::unique_lock<std::recursive_mutex> guard{writerThreadData_->mutex};
    writerThreadData_->maxTimestampProvider = maxTimestampProvider;
    writerThreadData_->autoCollectDelay.store(delay, std::memory_order_relaxed);
  } // mutex guard
  writeRecordsAsync(maxTimestampProvider());
  return 0;
}

int RecordFileWriter::autoPurgeRecords(function<double()> maxTimestampProvider, double delay) {
  if (purgeThreadData_) {
    std::unique_lock<std::recursive_mutex> guard{purgeThreadData_->mutex};
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
    purgeThreadData_->purgeThread =
        std::thread{&RecordFileWriter::backgroundPurgeThreadActivity, this};
  }
  return 0;
}

void RecordFileWriter::trackBackgroundThreadQueueByteSize() {
  if (!queueByteSize_) {
    queueByteSize_ = std::make_unique<std::atomic<uint64_t>>();
  }
}

uint64_t RecordFileWriter::getBackgroundThreadQueueByteSize() {
  return queueByteSize_ ? queueByteSize_->load(memory_order_relaxed) : 0;
}

int RecordFileWriter::closeFileAsync() {
  if (!writerThreadData_) {
    return NO_FILE_OPEN;
  }
  if (!writerThreadData_->shouldEndThread) {
    if (LOG_FILE_OPERATIONS) {
      XR_LOGD("File close request received.");
    }
    for (auto recordable : getRecordables()) {
      recordable->getRecordManager().purgeCache();
    }
    writeRecordsAsync(Record::kMaxTimestamp);
    writerThreadData_->shouldEndThread = true;
    writerThreadData_->writeEventChannel.dispatchEvent();
  }
  return writerThreadData_->fileError; // if there was some error already, say so
}

int RecordFileWriter::waitForFileClosed() {
  if (!writerThreadData_) {
    return NO_FILE_OPEN;
  }
  closeFileAsync();
  writerThreadData_->saveThread.join();
  int chunkError = 0;
  if (newChunkHandler_) {
    newChunkHandler_.reset();
  }
  // free all record memory
  for (auto recordable : getRecordables()) {
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
    for (auto tag : newTags) {
      fileTags_[tag.first] = tag.second;
    }
  }
}

int RecordFileWriter::setWriteFileHandler(unique_ptr<WriteFileHandler> writeFileHandler) {
  if (isWriting()) {
    return FILE_ALREADY_OPEN;
  }
  file_ = move(writeFileHandler);
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
  if (!writerThreadData_->hasRecordsReadyToWrite.load(std::memory_order_relaxed)) {
    return false;
  }
  RecordBatches batches;
  { // mutex guard
    std::unique_lock<std::recursive_mutex> guard{writerThreadData_->mutex};
    batches.swap(writerThreadData_->recordsReadyToWrite);
    writerThreadData_->hasRecordsReadyToWrite.store(false, std::memory_order_relaxed);
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
  RecordList(const pair<StreamId, list<Record*>>& deviceRecords)
      : deviceRecords_{&deviceRecords}, iter_{deviceRecords.second.begin()} {}
  RecordList(const RecordList& rhs) = default;

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

#define IF_ERROR_LOG_CLOSE_AND_RETURN(operation__) \
  {                                                \
    int operationError__ = operation__;            \
    if (operationError__ != 0) {                   \
      XR_LOGE(                                     \
          "{} failed: {}, {}",                     \
          #operation__,                            \
          operationError__,                        \
          errorCodeToMessage(operationError__));   \
      head.close();                                \
      return operationError__;                     \
    }                                              \
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
    // If you are not using DiskFile, we assume you are writing to a remote storage.
    // In that case, the file needs a split head, so the VRS file's head can be written
    // to a temporary local file (it needs to be edited during creation), while the rest of the
    // payload can be uploaded. The file's head will be uploaded & prepended to the uploaded file
    // on file close.
    if (spec.chunks.size() < 2) {
      XR_LOGE(
          "To write files with {}, you need to have at least 2 chunks "
          "['file head temporary local path', 'upload destination']...",
          spec.fileHandlerName);
      return INVALID_FILE_SPEC;
    }
    splitHead = true;

    std::unique_ptr<WriteFileHandler> writeFile{dynamic_cast<WriteFileHandler*>(
        FileHandlerFactory::getInstance().getFileHandler(spec.fileHandlerName).release())};
    if (!writeFile) {
      XR_LOGE("Found no WriteFileHandler named {}.", spec.fileHandlerName);
      return INVALID_FILE_SPEC;
    }

    file_ = std::move(writeFile);
  } else if (spec.chunks.size() != 1) {
    XR_LOGE("File creation using {} requires a single file chunk.", spec.fileHandlerName);
    return INVALID_FILE_SPEC;
  }

  WriteFileHandler& head = splitHead ? indexRecordWriter_.initSplitHead() : *file_;
  error = head.create(spec.chunks.front());
  if (error != 0) {
    XR_LOGW("createFile '{}' failed: {}, {}", filePath, error, errorCodeToMessage(error));
    return error;
  }
  fileHeader_.init();
  fileHeader_.descriptionRecordOffset.set(sizeof(fileHeader_));
  IF_ERROR_LOG_CLOSE_AND_RETURN(head.write(fileHeader_))
  map<StreamId, const StreamTags*> streamTags;
  for (auto* recordable : getRecordables()) {
    streamTags[recordable->getStreamId()] = &(recordable->getRecordableTags());
    indexRecordWriter_.addStream(recordable->getStreamId());
  }
  lastRecordSize_ = 0;
  IF_ERROR_LOG_CLOSE_AND_RETURN(
      DescriptionRecord::writeDescriptionRecord(head, streamTags, fileTags_, lastRecordSize_))

  if (splitHead) {
    IF_ERROR_LOG_CLOSE_AND_RETURN(indexRecordWriter_.createSplitIndexRecord(lastRecordSize_))
    // create the (first) user record chunk
    if (spec.chunks.size() == 1) {
      IF_ERROR_LOG_CLOSE_AND_RETURN(file_->create(spec.chunks.front() + "_1"))
    } else {
      IF_ERROR_LOG_CLOSE_AND_RETURN(file_->create(filePath))
    }
  } else if (preliminaryIndex_ && !preliminaryIndex_->empty()) {
    // only use this preliminary index once
    unique_ptr<deque<IndexRecord::DiskRecordInfo>> index = move(preliminaryIndex_);
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

int RecordFileWriter::writeRecordsSingleThread(SortedRecords& records, int lastError) {
  if (LOG_FILE_OPERATIONS) {
    XR_LOGD("Starting to write {} records", records.size());
  }
  int error = lastError;
  uint64_t currentChunkSize = static_cast<uint64_t>(file_->getChunkPos());
  double oldest = DBL_MAX;
  double newest = -DBL_MAX;
  uint32_t writtenRecords = 0;
  uint32_t skippedRecords = 0;
  for (auto& r : records) {
    Record* record = r.record;
    if (error != 0) {
      skippedRecords++;
    } else {
      double timestamp = record->getTimestamp();
      if (timestamp < oldest) {
        oldest = timestamp;
      }
      if (timestamp > newest) {
        newest = timestamp;
      }
      if (currentChunkSize > 0 && currentChunkSize + record->getSize() >= maxChunkSize_) {
        NewChunkNotifier newChunkNotifier(*file_, newChunkHandler_);
        // AddChunk() preserves the current chunk on error.
        XR_VERIFY(
            file_->addChunk() == 0,
            "Add chunk failed: {}, {}",
            file_->getLastError(),
            errorCodeToMessage(file_->getLastError()));
        currentChunkSize = 0; // reset, even if addChunk() failed to reduce retrying.
        newChunkNotifier.notify(1); // offset chunk index by 1
      }
      if (queueByteSize_) {
        queueByteSize_->fetch_sub(record->getSize(), memory_order_relaxed);
      }
      error = record->compressAndWriteRecord(*file_, r.streamId, lastRecordSize_, compressor_);
      if (error != 0) {
        XR_LOGE("Write failed: {}, {}", error, errorCodeToMessage(error));
      } else {
        writtenRecords++;
        indexRecordWriter_.addRecord(
            record->getTimestamp(), lastRecordSize_, r.streamId, record->getRecordType());
        currentChunkSize += lastRecordSize_;
      }
    }
    record->recycle();
  }
  if (LOG_FILE_OPERATIONS) {
    // If an error occurred, it was already logged. Log success & (otherwise) silent skipping of
    // write operations...
    if (writtenRecords > 0) {
      if (writtenRecords == records.size()) {
        XR_LOGD("Wrote all {} records, from {} to {}", writtenRecords, oldest, newest);
      } else {
        XR_LOGW(
            "Wrote {} out of {} records, from {} to {}",
            writtenRecords,
            records.size(),
            oldest,
            newest);
      }
    }
    if (skippedRecords > 0) {
      if (skippedRecords == records.size()) {
        XR_LOGW("Skipped all {} records, from {} to {}", skippedRecords, oldest, newest);
      } else {
        XR_LOGW(
            "Skipped {} out of {} records, from {} to {}",
            skippedRecords,
            records.size(),
            oldest,
            newest);
      }
    }
  }
  records.clear();
  return error;
}

int RecordFileWriter::writeRecordsMultiThread(
    CompressionThreadsData& compressionThreadsData,
    SortedRecords& recordsToCompress,
    int lastError) {
  uint64_t recordsToWriteCount = recordsToCompress.size();
  CompressionJob noCompressionJob;
  vector<CompressionJob> jobs(compressionThreadPoolSize_ * 3);
  list<CompressionJob*> availableJobs;
  for (auto& job : jobs) {
    availableJobs.push_back(&job);
  }
  deque<SortRecord> writeQueue; // we need to preserve the order when writing data out!
  map<SortRecord, CompressionJob*> compressionResults;
  uint64_t currentChunkSize = static_cast<uint64_t>(file_->getChunkPos());
  int error = lastError;
  double oldest = DBL_MAX;
  double newest = -DBL_MAX;
  uint64_t writtenRecords = 0;
  uint64_t skippedRecords = 0;
  uint64_t compressedRecords = 0;
  while (!recordsToCompress.empty() || !writeQueue.empty() || !compressionResults.empty()) {
    double waitTime = 10;
    // See if we can find a new compressor for that job
    while (!recordsToCompress.empty() && !availableJobs.empty()) {
      SortRecord nextRecord = *recordsToCompress.begin();
      recordsToCompress.erase(recordsToCompress.begin());
      writeQueue.push_back(nextRecord);
      if (error == 0 && nextRecord.record->shouldTryToCompress()) {
        compressionThreadsData.addThreadUntil(
            compressionThreadPoolSize_, initCreatedThreadCallback_);
        // an idle worker might not have any job available (they haven't been written yet)
        CompressionJob* job = availableJobs.front();
        availableJobs.pop_front();
        job->setSortRecord(nextRecord);
        compressionThreadsData.jobsQueue.sendJob(job);
        compressedRecords++;
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
      const StreamId id = resultsIter->first.streamId;
      CompressionJob* job = resultsIter->second;
      if (error != 0) {
        skippedRecords++;
      } else {
        double timestamp = record->getTimestamp();
        if (timestamp < oldest) {
          oldest = timestamp;
        }
        if (timestamp > newest) {
          newest = timestamp;
        }
        if (currentChunkSize > 0 && currentChunkSize + record->getSize() >= maxChunkSize_) {
          NewChunkNotifier newChunkNotifier(*file_, newChunkHandler_);
          // AddChunk() preserves the current chunk on error.
          XR_VERIFY(
              file_->addChunk() == 0,
              "Add chunk failed: {}, {}",
              file_->getLastError(),
              errorCodeToMessage(file_->getLastError()));
          currentChunkSize = 0; // reset, even if addChunk() failed to reduce retrying.
          newChunkNotifier.notify(1); // offset chunk index by 1
        }
        if (queueByteSize_) {
          queueByteSize_->fetch_sub(record->getSize(), memory_order_relaxed);
        }
        error = record->writeRecord(
            *file_, id, lastRecordSize_, job->getCompressor(), job->getCompressedSize());
        if (error != 0) {
          XR_LOGE("Write failed: {}, {}", error, errorCodeToMessage(error));
        } else {
          indexRecordWriter_.addRecord(
              record->getTimestamp(), lastRecordSize_, id, record->getRecordType());
          writtenRecords++;
          currentChunkSize += lastRecordSize_;
        }
      }
      record->recycle();
      if (job != &noCompressionJob) {
        availableJobs.push_back(job);
      }
      compressionResults.erase(resultsIter);
      writeQueue.pop_front();
      waitTime = 0;
    }
    // Check if we have a results to process
    CompressionJob* job;
    while (compressionThreadsData.resultsQueue.waitForJob(job, waitTime)) {
      compressionResults.emplace(job->getSortRecord(), job);
      waitTime = 0;
    }
    // Grab any new record ready to write, to feed our background threads ASAP
    size_t previousCount = recordsToCompress.size();
    if (addRecordsReadyToWrite(recordsToCompress)) {
      recordsToWriteCount += recordsToCompress.size() - previousCount;
    }
  }

  if (LOG_FILE_OPERATIONS) {
    // If an error occurred, it was already logged. Log success & (otherwise) silent skipping of
    // write operations...
    if (writtenRecords > 0) {
      if (writtenRecords == recordsToWriteCount) {
        XR_LOGD(
            "Wrote all {} records, compressed {}, using {} threads, from {} to {}",
            writtenRecords,
            compressedRecords,
            compressionThreadsData.compressionThreadsPool_.size(),
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
  return error;
}

int RecordFileWriter::completeAndCloseFile() {
  if (!isWriting()) {
    return NO_FILE_OPEN;
  }
  int error = 0;
  if (!skipFinalizeIndexRecords_) {
    if (indexRecordWriter_.hasSplitHead()) {
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
  if (writerThreadData_) {
    waitForFileClosed();
    delete writerThreadData_;
  }
  if (purgeThreadData_) {
    purgeThreadData_->shouldEndThread = true;
    purgeThreadData_->purgeEventChannel.dispatchEvent();
    purgeThreadData_->purgeThread.join();
    delete purgeThreadData_;
  }
}

} // namespace vrs
