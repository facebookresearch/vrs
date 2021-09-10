//  Facebook Technologies, LLC Proprietary and Confidential.

#include "ImageFilter.h"

#define DEFAULT_LOG_CHANNEL "ImageFilter"
#include <logging/Log.h>

#include <vrs/RecordFormatStreamPlayer.h>
#include <vrs/helpers/JobQueue.h>

#include "CopyHelpers.h"

using namespace std;
using namespace vrs;

namespace vrs::utils {

size_t ImageFilter::getThreadCount() {
  return 1;
}

static int filterImagesMT(
    ImageFilter& imageFilter,
    FilteredVRSFileReader& filteredReader,
    ThrottledWriter& throttledWriter,
    const string& pathToCopy,
    CopyOptions& copyOptions,
    std::unique_ptr<UploadMetadata>&& uploadMetadata);

namespace {

class ImageFilterChunk : public ContentBlockChunk {
 public:
  ImageFilterChunk(
      const IndexRecord::RecordInfo& recordInfo,
      size_t blockIndex,
      const ContentBlock& contentBlock,
      ImageFilter& imageFilter,
      const CurrentRecord& record)
      : ContentBlockChunk(contentBlock, record),
        recordInfo_{recordInfo},
        blockIndex_{blockIndex},
        imageFilter_{imageFilter} {}

  size_t filterBuffer() override {
    vector<uint8_t> filteredBuffer(contentBlock_.getBlockSize());
    imageFilter_.filter(recordInfo_, blockIndex_, contentBlock_, getBuffer(), filteredBuffer);
    getBuffer().swap(filteredBuffer);
    return getBuffer().size();
  }

 private:
  const IndexRecord::RecordInfo recordInfo_;
  const size_t blockIndex_;
  ImageFilter& imageFilter_;
};

static IndexRecord::RecordInfo headerToRecordInfo(const CurrentRecord& record) {
  return IndexRecord::RecordInfo(
      record.timestamp, record.formatVersion, record.streamId, record.recordType);
}

class BufferSource : public DataSource {
 public:
  BufferSource(ContentChunk& chunk) : DataSource(chunk.filterBuffer()), chunk_{chunk} {}
  void copyTo(uint8_t* buffer) const override {
    chunk_.fillAndAdvanceBuffer(buffer);
  }

 private:
  ContentChunk& chunk_;
};

class RecordWriter : public vrs::Recordable {
 public:
  RecordWriter(vrs::RecordableTypeId typeId, const string& flavor) : Recordable(typeId, flavor) {}

  const vrs::Record* createStateRecord() override {
    return nullptr;
  }
  const vrs::Record* createConfigurationRecord() override {
    return nullptr;
  }
  const vrs::Record* createRecord(
      const vrs::IndexRecord::RecordInfo& r,
      std::vector<int8_t>& data) {
    return Recordable::createRecord(
        r.timestamp, r.recordType, static_cast<uint32_t>(r.fileOffset), DataSource(data));
  }
  const vrs::Record* createRecord(const vrs::IndexRecord::RecordInfo& r, vrs::DataSource& source) {
    return Recordable::createRecord(
        r.timestamp, r.recordType, static_cast<uint32_t>(r.fileOffset), source);
  }
};

class ImageFilterStreamPlayer : public RecordFormatStreamPlayer {
 public:
  ImageFilterStreamPlayer(
      ImageFilter& imageFilter,
      RecordFileReader& reader,
      RecordFileWriter& fileWriter,
      StreamId id,
      const CopyOptions& options)
      : imageFilter_{imageFilter},
        options_{options},
        writer_(id.getTypeId(), reader.getFlavor(id)),
        fileWriter_{fileWriter} {
    reader.setStreamPlayer(id, this);
    fileWriter.addRecordable(&writer_);
    // copy the tags of that stream
    writer_.addTags(reader.getTags(id));
    writer_.setCompression(options_.getCompression());
  }

  bool processRecordHeader(const CurrentRecord& record, DataReference& ref) override {
    if (record.recordSize == 0) {
      return true;
    }
    return RecordFormatStreamPlayer::processRecordHeader(record, ref);
  }

  void processRecord(const CurrentRecord& record, uint32_t readSize) override {
    chunks_.clear();
    if (record.recordSize > 0) {
      // Read all the parts, which will result in multiple onXXXRead() callbacks
      RecordFormatStreamPlayer::processRecord(record, readSize);
    }
    // filter & flush the collected data, in the order collected
    FilteredChunksSource chunkedSource(chunks_);
    writer_.createRecord(record, chunkedSource);
    ++options_.outRecordCopiedCount;
  }

  bool onDataLayoutRead(const CurrentRecord& record, size_t idx, DataLayout& datalayout) override {
    chunks_.emplace_back(make_unique<ContentChunk>(datalayout));
    return true; // we can go read the next block, if any, since we've read the data
  }

  bool onImageRead(const CurrentRecord&, size_t blockIndex, const ContentBlock& cb) override;

  bool onUnsupportedBlock(const CurrentRecord& record, size_t, const ContentBlock& cb) override {
    bool readNext = true;
    size_t blockSize = cb.getBlockSize();
    if (blockSize == ContentBlock::kSizeUnknown) {
      // just read everything left, without trying to analyse content
      blockSize = record.reader->getUnreadBytes();
      readNext = false;
    }
    unique_ptr<ContentChunk> bufferSourceChunk = make_unique<ContentChunk>(blockSize);
    /*int result =*/record.reader->read(bufferSourceChunk->getBuffer());
    chunks_.emplace_back(move(bufferSourceChunk));
    return readNext;
  }

 protected:
  ImageFilter& imageFilter_;
  const CopyOptions& options_;
  Writer writer_;
  RecordFileWriter& fileWriter_;
  deque<unique_ptr<ContentChunk>> chunks_;
};

bool ImageFilterStreamPlayer::onImageRead(
    const CurrentRecord& r,
    size_t idx,
    const ContentBlock& cb) {
  size_t blockSize = cb.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown || !(imageFilter_.accept(cb.image()))) {
    return onUnsupportedBlock(r, idx, cb);
  }
  IndexRecord::RecordInfo recordInfo{r.timestamp, 0, r.streamId, r.recordType};
  unique_ptr<ImageFilterChunk> imageChunk =
      make_unique<ImageFilterChunk>(recordInfo, idx, cb, imageFilter_, r);

  chunks_.emplace_back(move(imageChunk));
  return true;
}

} // namespace

int filterImages(
    ImageFilter& imageFilter,
    FilteredVRSFileReader& filteredReader,
    ThrottledWriter& throttledWriter,
    const string& pathToCopy,
    CopyOptions& copyOptions,
    std::unique_ptr<UploadMetadata>&& uploadMetadata) {
  if (imageFilter.getThreadCount() > 1) {
    return filterImagesMT(
        imageFilter,
        filteredReader,
        throttledWriter,
        pathToCopy,
        copyOptions,
        move(uploadMetadata));
  }
  if (!filteredReader.reader.isOpened()) {
    int status = filteredReader.openFile();
    if (status != 0) {
      return status;
    }
  }
  RecordFileWriter& writer = throttledWriter.getWriter();
  writer.addTags(filteredReader.reader.getTags());
  vector<unique_ptr<StreamPlayer>> copiers;
  for (auto id : filteredReader.filter.streams) {
    unique_ptr<StreamPlayer> streamPlayer;
    // Use ImageFilter if we find at least one image block defined
    RecordFormatMap formats;
    if (filteredReader.reader.getRecordFormats(id, formats) > 0) {
      for (const auto& format : formats) {
        if (format.second.getBlocksOfTypeCount(ContentType::IMAGE) > 0) {
          streamPlayer = make_unique<ImageFilterStreamPlayer>(
              imageFilter, filteredReader.reader, writer, id, copyOptions);
          break;
        }
      }
    }
    if (streamPlayer) {
      copiers.emplace_back(move(streamPlayer));
    } else {
      copiers.emplace_back(make_unique<Copier>(filteredReader.reader, writer, id, copyOptions));
    }
  }

  try {
    double startTimestamp, endTimestamp;
    filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
    if (!uploadMetadata) {
      writer.preallocateIndex(filteredReader.buildIndex());
    }
    ThrottledFileHelper fileHelper(throttledWriter);
    int result = fileHelper.createFile(pathToCopy, uploadMetadata);
    if (result == 0) {
      // Init tracker propgress early, to be sure we track the background thread queue size
      // make sure to copy most recent config & state records
      filteredReader.preRollConfigAndState();
      throttledWriter.initTimeRange(startTimestamp, endTimestamp);
      filteredReader.iterate(&throttledWriter);
      result = fileHelper.closeFile();
      if (writer.getBackgroundThreadQueueByteSize() != 0) {
        XR_LOGE("Unexpected count of bytes left in queue after copy!");
      }
    }
    if (result != 0) {
      XR_LOGE("ImageFilter file error #{}: {}", result, errorCodeToMessage(result));
    }
    return result;
  } catch (const exception& e) {
    XR_LOGE("Exception thrown during filtering: {}", e.what());
    throttledWriter.closeFile();
    return -1;
  }
}

// Multithreaded versions
namespace {
class Job {
 public:
  Job() : finalJob_{false} {}
  virtual ~Job() {}

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  virtual void performJob() = 0;

  // A final job ends the work thread it runs on after execution
  void makeFinalJob() {
    finalJob_ = true;
  }
  bool isFinalJob() const {
    return finalJob_;
  }

 private:
  bool finalJob_;
};

using FilterJobQueue = JobQueue<unique_ptr<Job>>;

class WorkerThread {
 public:
  WorkerThread(FilterJobQueue& jobQueue, size_t threadIndex)
      : jobQueue_{jobQueue},
        threadIndex_{threadIndex},
        thread_(&WorkerThread::threadActivity, this) {
    XR_LOGD("Starting image filter thread #{}", threadIndex_ + 1);
  }
  ~WorkerThread() {
    join();
    XR_LOGD("Image filter thread #{} ended.", threadIndex_ + 1);
  }

  void join() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  void threadActivity() {
    bool finalJob = false;
    while (!finalJob && !jobQueue_.hasEnded()) {
      unique_ptr<Job> job;
      if (jobQueue_.waitForJob(job, 10 /* second */)) {
        job->performJob();
        finalJob = job->isFinalJob();
      }
    }
  }

  FilterJobQueue& jobQueue_;
  size_t threadIndex_;
  thread thread_;
};

class EndWorkerThreadJob : public Job {
 public:
  EndWorkerThreadJob() {
    makeFinalJob();
  }

  void performJob() override {}
};

class WorkerThreadsPool {
 public:
  WorkerThreadsPool(size_t threadPoolSize) {
    if (workerThreadsPool_.size() < threadPoolSize) {
      workerThreadsPool_.reserve(threadPoolSize);
      while (workerThreadsPool_.size() < threadPoolSize) {
        workerThreadsPool_.emplace_back(new WorkerThread(jobsQueue, workerThreadsPool_.size()));
      }
    }
  }
  ~WorkerThreadsPool() {
    jobsQueue.endQueue();
    workerThreadsPool_.clear();
  }

  void finishQueue() {
    // make threads end "naturally", by adding end jobs at the end of the queue
    for (size_t k = workerThreadsPool_.size(); k > 0; k--) {
      jobsQueue.sendJob(make_unique<EndWorkerThreadJob>());
    }
    // now join all the threads, which should all have received an EndWorkerThreadJob.
    for (auto& worker : workerThreadsPool_) {
      worker->join();
    }
  }

  FilterJobQueue jobsQueue;

 private:
  vector<unique_ptr<WorkerThread>> workerThreadsPool_;
};

class RecordCreationJob : public Job {
 public:
  RecordCreationJob(
      RecordWriter& writer,
      unique_ptr<deque<unique_ptr<ContentChunk>>>& chunks,
      const IndexRecord::RecordInfo& recordInfo,
      RecordFileWriter& fileWriter)
      : writer_{writer}, recordInfo_{recordInfo}, fileWriter_{fileWriter} {
    chunks_.swap(chunks);
    jobPendingCount_++;
  }

  void performJob() override {
    FilteredChunksSource chunkedSource(*chunks_);
    writer_.createRecord(recordInfo_, chunkedSource);
    jobPendingCount_--;
    fileWriter_.writeRecordsAsync(recordInfo_.timestamp - 0.5);
  }
  static uint32_t getJobPendingCount() {
    return jobPendingCount_;
  }

 private:
  RecordWriter& writer_;
  unique_ptr<deque<unique_ptr<ContentChunk>>> chunks_;
  const IndexRecord::RecordInfo recordInfo_;
  RecordFileWriter& fileWriter_;
  static atomic<uint32_t> jobPendingCount_;
};

atomic<uint32_t> RecordCreationJob::jobPendingCount_{0};

class BufferRecordJob : public Job {
 public:
  BufferRecordJob(RecordWriter& writer, const IndexRecord::RecordInfo& recordInfo, size_t size)
      : writer_{writer}, recordInfo_{recordInfo}, bufferChunk_{size} {}

  vector<uint8_t>& getBuffer() {
    return bufferChunk_.getBuffer();
  }

  void performJob() override {
    BufferSource source(bufferChunk_);
    writer_.createRecord(recordInfo_, source);
  }

 private:
  RecordWriter& writer_;
  const IndexRecord::RecordInfo recordInfo_;
  ContentChunk bufferChunk_;
};

class ImageFilterStreamPlayerMT : public RecordFormatStreamPlayer {
 public:
  ImageFilterStreamPlayerMT(
      ImageFilter& imageFilter,
      RecordFileReader& reader,
      RecordFileWriter& fileWriter,
      FilterJobQueue& jobsQueue,
      StreamId id,
      const CopyOptions& options)
      : imageFilter_{imageFilter},
        options_{options},
        writer_(id.getTypeId(), reader.getFlavor(id)),
        fileWriter_{fileWriter},
        jobsQueue_{jobsQueue} {
    reader.setStreamPlayer(id, this);
    fileWriter.addRecordable(&writer_);
    // copy the tags of that stream
    writer_.addTags(reader.getTags(id));
    writer_.setCompression(options_.getCompression());
  }

  bool processRecordHeader(const CurrentRecord& record, DataReference& ref) override {
    if (record.recordSize == 0) {
      return true;
    }
    return RecordFormatStreamPlayer::processRecordHeader(record, ref);
  }

  void processRecord(const CurrentRecord& record, uint32_t readSize) override {
    chunks_ = make_unique<deque<unique_ptr<ContentChunk>>>();
    recordInfo_ = headerToRecordInfo(record);
    if (record.recordSize > 0) {
      // Read all the parts, which will result in multiple onXXXRead() callbacks
      RecordFormatStreamPlayer::processRecord(record, readSize);
    }
    // The record has been read, and chunks hold all the ContentChunks, which are self contained.
    jobsQueue_.sendJob(make_unique<RecordCreationJob>(writer_, chunks_, recordInfo_, fileWriter_));
    ++options_.outRecordCopiedCount;
  }

  bool onDataLayoutRead(const CurrentRecord&, size_t blockIndex, DataLayout& datalayout) override {
    chunks_->emplace_back(make_unique<ContentChunk>(datalayout));
    return true; // we can go read the next block, if any, since we've read the data
  }

  bool onImageRead(const CurrentRecord& record, size_t blockIndex, const ContentBlock& cb) override;

  bool onUnsupportedBlock(const CurrentRecord& rec, size_t idx, const ContentBlock& cb) override {
    bool readNext = true;
    size_t blockSize = cb.getBlockSize();
    if (blockSize == ContentBlock::kSizeUnknown) {
      // just read everything left, without trying to analyse content
      blockSize = rec.reader->getUnreadBytes();
      readNext = false;
    }
    unique_ptr<ContentChunk> bufferSourceChunk = make_unique<ContentChunk>(blockSize);
    /*int result =*/rec.reader->read(bufferSourceChunk->getBuffer());
    chunks_->emplace_back(move(bufferSourceChunk));
    return readNext;
  }

 protected:
  ImageFilter& imageFilter_;
  const CopyOptions& options_;
  RecordWriter writer_;
  RecordFileWriter& fileWriter_;
  FilterJobQueue& jobsQueue_;
  // these two are only valid while a record is being read by processRecord, in particular, during
  // the onXXXRead() callbacks.
  IndexRecord::RecordInfo recordInfo_;
  unique_ptr<deque<unique_ptr<ContentChunk>>> chunks_;
};

bool ImageFilterStreamPlayerMT::onImageRead(
    const CurrentRecord& record,
    size_t blockIndex,
    const ContentBlock& cb) {
  size_t blockSize = cb.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown || !(imageFilter_.accept(cb.image()))) {
    return onUnsupportedBlock(record, blockIndex, cb);
  }
  IndexRecord::RecordInfo recordInfo{
      record.timestamp, record.formatVersion, record.streamId, record.recordType};
  unique_ptr<ImageFilterChunk> imageChunk =
      make_unique<ImageFilterChunk>(recordInfo, blockIndex, cb, imageFilter_, record);

  chunks_->emplace_back(move(imageChunk));
  return true;
}

class CopierMT : public vrs::StreamPlayer {
 public:
  CopierMT(
      vrs::RecordFileReader& reader,
      vrs::RecordFileWriter& fileWriter,
      FilterJobQueue& jobsQueue,
      vrs::StreamId id,
      const CopyOptions& copyOptions)
      : writer_(id.getTypeId(), reader.getFlavor(id)),
        fileWriter_{fileWriter},
        jobsQueue_{jobsQueue},
        options_{copyOptions} {
    reader.setStreamPlayer(id, this);
    fileWriter.addRecordable(&writer_);
    // copy the tags of that stream
    writer_.addTags(reader.getTags(id));
    writer_.setCompression(options_.getCompression());
  }

  bool processRecordHeader(const CurrentRecord& record, vrs::DataReference& outDataRef) override {
    bufferJob_ =
        make_unique<BufferRecordJob>(writer_, headerToRecordInfo(record), record.recordSize);
    outDataRef.useRawData(bufferJob_->getBuffer().data(), record.recordSize);
    return true;
  }

  void processRecord(const CurrentRecord& record, uint32_t bytesWrittenCount) override {
    jobsQueue_.sendJob(move(bufferJob_));
    ++options_.outRecordCopiedCount;
  }

 protected:
  RecordWriter writer_;
  vrs::RecordFileWriter& fileWriter_;
  FilterJobQueue& jobsQueue_;
  const CopyOptions& options_;
  unique_ptr<BufferRecordJob> bufferJob_;
};
} // namespace

int filterImagesMT(
    ImageFilter& imageFilter,
    FilteredVRSFileReader& filteredReader,
    ThrottledWriter& throttledWriter,
    const string& pathToCopy,
    CopyOptions& copyOptions,
    std::unique_ptr<UploadMetadata>&& uploadMetadata = nullptr) {
  copyOptions.graceWindow = 1;
  if (!filteredReader.reader.isOpened()) {
    int status = filteredReader.openFile();
    if (status != 0) {
      return status;
    }
  }
  WorkerThreadsPool workerPool(imageFilter.getThreadCount());
  function<bool()> condition = [] { return RecordCreationJob::getJobPendingCount() > 50; };
  throttledWriter.addWaitCondition(condition);
  RecordFileWriter& writer = throttledWriter.getWriter();
  writer.addTags(filteredReader.reader.getTags());
  vector<unique_ptr<StreamPlayer>> copiers;
  for (auto id : filteredReader.filter.streams) {
    unique_ptr<StreamPlayer> streamPlayer;
    // Use ImageFilter if we find at least one image block defined
    RecordFormatMap formats;
    if (filteredReader.reader.getRecordFormats(id, formats) > 0) {
      for (const auto& format : formats) {
        if (format.second.getBlocksOfTypeCount(ContentType::IMAGE) > 0) {
          streamPlayer = make_unique<ImageFilterStreamPlayerMT>(
              imageFilter, filteredReader.reader, writer, workerPool.jobsQueue, id, copyOptions);
          break;
        }
      }
    }
    if (streamPlayer) {
      copiers.emplace_back(move(streamPlayer));
    } else {
      copiers.emplace_back(make_unique<CopierMT>(
          filteredReader.reader, writer, workerPool.jobsQueue, id, copyOptions));
    }
  }

  try {
    double startTimestamp, endTimestamp;
    filteredReader.getConstrainedTimeRange(startTimestamp, endTimestamp);
    if (!uploadMetadata) {
      writer.preallocateIndex(filteredReader.buildIndex());
    }
    ThrottledFileHelper fileHelper(throttledWriter);
    int result = fileHelper.createFile(pathToCopy, uploadMetadata);
    if (result == 0) {
      // Init tracker propgress early, to be sure we track the background thread queue size
      // make sure to copy most recent config & state records
      filteredReader.preRollConfigAndState();
      throttledWriter.initTimeRange(startTimestamp, endTimestamp);
      filteredReader.iterate(&throttledWriter);
      workerPool.finishQueue();
      result = fileHelper.closeFile();
      if (writer.getBackgroundThreadQueueByteSize() != 0) {
        XR_LOGE("Unexpected count of bytes left in queue after copy!");
      }
    }
    if (result != 0) {
      XR_LOGE("ImageFilter file error #{}: {}", result, errorCodeToMessage(result));
    }
    return result;
  } catch (const exception& e) {
    XR_LOGE("Exception thrown during filtering: {}", e.what());
    throttledWriter.closeFile();
    return -1;
  }
}

} // namespace vrs::utils
