// Facebook Technologies, LLC Proprietary and Confidential.

#include "CopyHelpers.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <thread>

#define DEFAULT_LOG_CHANNEL "CopyHelpers"
#include <logging/Log.h>
#include <portability/Platform.h>

#include <vrs/FileHandlerFactory.h>
#include <vrs/RapidjsonHelper.hpp>
#include <vrs/RecordFileInfo.h>
#include <vrs/RecordFileWriter.h>
#include <vrs/StreamPlayer.h>
#include <vrs/gaia/CachedGaiaFileHandler.h>
#include <vrs/gaia/GaiaClient.h>
#include <vrs/gaia/support/GaiaClientConfig.h>
#include <vrs/os/Time.h>
#include <vrs/os/Utils.h>

using namespace std;
using namespace vrs;

namespace vrs::utils {

const size_t kDownloadChunkSize = 1024 * 1024 * 4;

#if IS_WINDOWS_PLATFORM()
// "\r" works, but escape sequences don't by default: overwrite with
// white spaces... :-(
const char* const kResetCurrentLine = "\r                                            \r";
#else
const char* const kResetCurrentLine = "\r\33[2K\r";
#endif

void printProgress(const char* status, size_t currentSize, size_t totalSize, bool showProgress) {
  if (showProgress) {
    size_t percent = 100 * currentSize / totalSize;
    cout << kResetCurrentLine << status << setw(2) << percent << "%...";
    cout.flush();
  }
}

class ProgressPrinter {
 public:
  ProgressPrinter(bool showProgress) : showProgress_{showProgress} {}
  ~ProgressPrinter() {
    clear();
  }
  void clear() {
    if (showProgress_) {
      cout << kResetCurrentLine;
      cout.flush();
    }
  }
  void show(const char* message) {
    if (showProgress_) {
      cout << kResetCurrentLine << message;
      cout.flush();
    }
  }
  void show(const char* status, size_t currentSize, size_t totalSize) {
    printProgress(status, currentSize, totalSize, showProgress_);
  }

 private:
  bool showProgress_;
};

const Record* Writer::createStateRecord() {
  return nullptr;
}

const Record* Writer::createConfigurationRecord() {
  return nullptr;
}

const Record* Writer::createRecord(const CurrentRecord& record, std::vector<int8_t>& data) {
  return Recordable::createRecord(
      record.timestamp, record.recordType, record.formatVersion, DataSource(data));
}

const Record* Writer::createRecord(const CurrentRecord& record, DataSource& source) {
  return Recordable::createRecord(
      record.timestamp, record.recordType, record.formatVersion, source);
}

const Record*
Writer::createRecord(double timestamp, Record::Type type, uint32_t formatVersion, DataSource& src) {
  return Recordable::createRecord(timestamp, type, formatVersion, src);
}

Copier::Copier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId id,
    const CopyOptions& copyOptions)
    : writer_(id.getTypeId(), fileReader.getFlavor(id)),
      fileWriter_{fileWriter},
      options_{copyOptions} {
  fileReader.setStreamPlayer(id, this);
  fileWriter.addRecordable(&writer_);
  // copy the tags of that stream
  writer_.addTags(fileReader.getTags(id));
  writer_.setCompression(options_.getCompression());
}

bool Copier::processRecordHeader(const CurrentRecord& record, DataReference& outDataRef) {
  rawRecordData_.resize(record.recordSize);
  outDataRef.useRawData(rawRecordData_.data(), record.recordSize);
  return true;
}

void Copier::processRecord(const CurrentRecord& record, uint32_t /*bytesWrittenCount*/) {
  writer_.createRecord(record, rawRecordData_);
  ++options_.outRecordCopiedCount;
}

ContentBlockChunk::ContentBlockChunk(const ContentBlock& contentBlock, const CurrentRecord& record)
    : ContentChunk(contentBlock.getBlockSize()), contentBlock_{contentBlock} {
  int status = record.reader->read(getBuffer());
  if (status != 0) {
    XR_LOGW("Failed to read image block: {}", errorCodeToMessage(status));
  }
}

ContentBlockChunk::ContentBlockChunk(const ContentBlock& contentBlock, vector<uint8_t>&& buffer)
    : ContentChunk(move(buffer)), contentBlock_{contentBlock} {}

RecordFilterCopier::RecordFilterCopier(
    RecordFileReader& fileReader,
    RecordFileWriter& fileWriter,
    StreamId id,
    const CopyOptions& copyOptions)
    : writer_(id.getTypeId(), fileReader.getFlavor(id)),
      fileWriter_{fileWriter},
      options_{copyOptions} {
  fileReader.setStreamPlayer(id, this);
  fileWriter.addRecordable(&writer_);
  // copy the tags of that stream
  writer_.addTags(fileReader.getTags(id));
  writer_.setCompression(options_.getCompression());
}

bool RecordFilterCopier::processRecordHeader(const CurrentRecord& rec, DataReference& outDataRef) {
  copyVerbatim_ = (rec.recordSize == 0) || shouldCopyVerbatim(rec);
  skipRecord_ = false;
  if (copyVerbatim_) {
    verbatimRecordData_.resize(rec.recordSize);
    outDataRef.useRawData(verbatimRecordData_.data(), rec.recordSize);
    return true;
  } else {
    return RecordFormatStreamPlayer::processRecordHeader(rec, outDataRef);
  }
}

void RecordFilterCopier::processRecord(const CurrentRecord& record, uint32_t readSize) {
  if (!copyVerbatim_) {
    // Read all the parts, which will result in multiple onXXXRead() callbacks
    chunks_.clear();
    RecordFormatStreamPlayer::processRecord(record, readSize);
  }
  finishRecordProcessing(record);
  ++options_.outRecordCopiedCount;
}

void RecordFilterCopier::finishRecordProcessing(const CurrentRecord& record) {
  if (!skipRecord_) {
    if (copyVerbatim_) {
      writer_.createRecord(record, verbatimRecordData_);
    } else {
      // filter & flush the collected data, in the order collected
      FilteredChunksSource chunkedSource(chunks_);
      CurrentRecord modifiedHeader(record);
      doHeaderEdits(modifiedHeader);
      writer_.createRecord(modifiedHeader, chunkedSource);
    }
  }
}

bool RecordFilterCopier::onDataLayoutRead(const CurrentRecord& rec, size_t index, DataLayout& dl) {
  dl.stageCurrentValues();
  doDataLayoutEdits(rec, index, dl);
  pushDataLayout(dl);
  return true;
}

bool RecordFilterCopier::onImageRead(const CurrentRecord& rec, size_t idx, const ContentBlock& cb) {
  size_t blockSize = cb.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown) {
    return onUnsupportedBlock(rec, idx, cb);
  }
  unique_ptr<ContentBlockChunk> imageChunk = make_unique<ContentBlockChunk>(cb, rec);
  filterImage(rec, idx, cb, imageChunk->getBuffer());
  chunks_.emplace_back(move(imageChunk));
  return true;
}

bool RecordFilterCopier::onAudioRead(const CurrentRecord& rec, size_t idx, const ContentBlock& cd) {
  size_t blockSize = cd.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown) {
    return onUnsupportedBlock(rec, idx, cd);
  }
  unique_ptr<ContentBlockChunk> audioChunk = make_unique<ContentBlockChunk>(cd, rec);
  filterAudio(rec, idx, cd, audioChunk->getBuffer());
  chunks_.emplace_back(move(audioChunk));
  return true;
}

bool RecordFilterCopier::onUnsupportedBlock(
    const CurrentRecord& record,
    size_t idx,
    const ContentBlock& cb) {
  bool readNext = true;
  size_t blockSize = cb.getBlockSize();
  if (blockSize == ContentBlock::kSizeUnknown) {
    // just read everything left, without trying to analyse content
    blockSize = record.reader->getUnreadBytes();
    readNext = false;
  }
  unique_ptr<ContentChunk> bufferSourceChunk = make_unique<ContentChunk>(blockSize);
  int status = record.reader->read(bufferSourceChunk->getBuffer());
  if (status != 0) {
    XR_LOGW("Failed to read {} block: {}", cb.asString(), errorCodeToMessage(status));
  }
  chunks_.emplace_back(move(bufferSourceChunk));
  return readNext;
}

void RecordFilterCopier::pushDataLayout(DataLayout& datalayout) {
  datalayout.collectVariableDataAndUpdateIndex();
  chunks_.emplace_back(make_unique<ContentChunk>(datalayout));
}

namespace {
#if IS_LINUX_PLATFORM() || IS_MAC_PLATFORM() || IS_WINDOWS_PLATFORM()
// No need to be stingy on memory usage on desktop platforms
const size_t kMaxQueueByteSize = 2 * 1024 * 1024 * 1024ULL; // 2 GB
#else
// Mobile environments are constrained, and might kill a greedy app...
const size_t kMaxQueueByteSize = 600 * 1024 * 1024; // 600 MB
#endif
const size_t kReadAgainQueueByteSize = kMaxQueueByteSize * 9 / 10; // 90%
const size_t kLowQueueByteSize = 40 * 1024 * 1024ULL;

const double kRefreshDelaySec = 1. / 3; // limit how frequently we show updates
} // namespace

ThrottledWriter::ThrottledWriter(const CopyOptions& options) : copyOptions_{options} {
  writer_.trackBackgroundThreadQueueByteSize();
  initWriter();
  nextUpdateTime_ = 0;
}

void ThrottledWriter::initWriter() {
  writer_.setCompressionThreadPoolSize(
      std::min<size_t>(copyOptions_.compressionPoolSize, thread::hardware_concurrency()));
  writer_.setMaxChunkSizeMB(copyOptions_.maxChunkSizeMB);
}

vrs::RecordFileWriter& ThrottledWriter::getWriter() {
  return writer_;
}

void ThrottledWriter::initTimeRange(double minTimestamp, double maxTimestamp) {
  minTimestamp_ = minTimestamp;
  duration_ = maxTimestamp - minTimestamp;
}

void ThrottledWriter::onRecordDecoded(double timestamp, double writeGraceWindow) {
  uint64_t queueByteSize = writer_.getBackgroundThreadQueueByteSize();
  const uint32_t writeInterval = copyOptions_.outRecordCopiedCount < 100 ? 10 : 100;
  if (queueByteSize == 0 || copyOptions_.outRecordCopiedCount % writeInterval == 0) {
    writer_.writeRecordsAsync(timestamp - max<double>(writeGraceWindow, copyOptions_.graceWindow));
  }
  // don't go crazy with memory usage, if we read data much faster than we can process it...
  if (queueByteSize > kMaxQueueByteSize || (waitCondition_ && waitCondition_())) {
    writer_.writeRecordsAsync(timestamp - max<double>(writeGraceWindow, copyOptions_.graceWindow));
    // wait that most of the buffers are processed to resume,
    // limiting collisions between input & output file operations.
    do {
      printPercentAndQueueSize(queueByteSize, true);
      std::this_thread::sleep_for(std::chrono::duration<double>(kRefreshDelaySec));
      queueByteSize = writer_.getBackgroundThreadQueueByteSize();
    } while (queueByteSize > kReadAgainQueueByteSize || (waitCondition_ && waitCondition_()));
    if (showProgress()) {
      cout << kResetCurrentLine;
      nextUpdateTime_ = 0;
    }
  }
  if (showProgress()) {
    double now = os::getTimestampSec();
    if (now >= nextUpdateTime_) {
      double progress = duration_ > 0.0001 ? (timestamp - minTimestamp_) / duration_ : 1.;
      // timestamp ranges only include data records, but config & state records might be beyond
      percent_ = std::max<int32_t>(static_cast<int32_t>(progress * 100), 0);
      percent_ = std::min<int32_t>(percent_, 100);
      printPercentAndQueueSize(writer_.getBackgroundThreadQueueByteSize(), false);
      nextUpdateTime_ = now + kRefreshDelaySec;
    }
  }
}

void ThrottledWriter::printPercentAndQueueSize(uint64_t queueByteSize, bool waiting) {
  if (showProgress()) {
    if (writer_.isWriting()) {
      cout << kResetCurrentLine << (waiting ? "Waiting " : "Reading ") << setw(2) << percent_
           << "%, processing " << setw(7) << RecordFileInfo::humanReadableFileSize(queueByteSize);
    } else {
      cout << kResetCurrentLine << "Reading " << setw(2) << percent_ << "%";
    }
    cout.flush();
  }
}

int ThrottledWriter::closeFile() {
  if (showProgress()) {
    writer_.closeFileAsync(); // non-blocking
    waitForBackgroundThreadQueueSize(kLowQueueByteSize / 3);
  }
  int copyResult = writer_.waitForFileClosed(); // blocking call
  if (showProgress()) {
    cout << kResetCurrentLine;
  }
  return copyResult;
}

void ThrottledWriter::waitForBackgroundThreadQueueSize(size_t maxSize) {
  if (showProgress()) {
    cout << kResetCurrentLine;
  }
  // To avoid stalls, don't wait quite until we have nothing left to process,
  uint64_t queueByteSize;
  while ((queueByteSize = writer_.getBackgroundThreadQueueByteSize()) > maxSize) {
    if (showProgress()) {
      cout << kResetCurrentLine << "Processing " << setw(7)
           << RecordFileInfo::humanReadableFileSize(queueByteSize);
      cout.flush();
    }
    // Check more frequently when we're getting close. This is Science.
    const double sleepDuration = queueByteSize > 3 * kLowQueueByteSize ? kRefreshDelaySec
        : queueByteSize > kLowQueueByteSize                            ? kRefreshDelaySec / 2
                                                                       : kRefreshDelaySec / 5;
    std::this_thread::sleep_for(std::chrono::duration<double>(sleepDuration));
  }
  if (showProgress()) {
    cout << kResetCurrentLine << "Finishing...";
    cout.flush();
  }
}

int ThrottledFileHelper::createFile(
    const string& pathToCopy,
    unique_ptr<UploadMetadata>& uploadMetadata) {
  RecordFileWriter& writer = throttledWriter_.getWriter();

  if (uploadMetadata) {
    uploadMetadata->setFileName(os::getFilename(pathToCopy));
    uploader_ = std::make_unique<GaiaUploader>(uploadMetadata->getUploadDestination());
    const size_t kMB = 1024 * 1024;
    const size_t kMaxBufferSize =
        2 * GaiaClientConfig::getInstance().getMaxUploadLocalCacheMB() * kMB;
    throttledWriter_.addWaitCondition(
        [this, kMaxBufferSize] { return uploader_->getQueueSize() >= kMaxBufferSize; });
    return uploader_->stream(move(uploadMetadata), writer, pathToCopy, uploadId_);
  }

  FileSpec spec;
  int status = spec.fromPathJsonUri(pathToCopy);

  if (status) {
    XR_LOGE("Failed to parse path: {}", pathToCopy);
    return status;
  }

  return writer.createFileAsync(pathToCopy);
}

int ThrottledFileHelper::closeFile() {
  int status = throttledWriter_.closeFile();
  if (uploader_) {
    int uploadStatus = uploader_->finishUpload(uploadId_, gaiaId_);
    if (status == 0) {
      status = uploadStatus;
    }
  }
  return status;
}

// helper function to avoid ignoring errors and report the first error that happened
static int compoundError(int error, int newError) {
  return error != 0 ? error : newError;
}

int verbatimDownload(
    GaiaIdFileVersion idv,
    const string& downloadLocation,
    bool showProgress,
    bool jsonOutput) {
  ProgressPrinter progress(showProgress);
  const char* kProgressMessage = "Downloading ";
  double timeBefore = os::getTimestampSec();
  int statusCode = 0;
  std::unique_ptr<GaiaClient> gaiaClient = GaiaClient::makeInstance();
  std::unique_ptr<FileHandler> file;
  if (showProgress) {
    cout << "Opening " << idv.toUri() << "...";
    cout.flush();
  }
  int errorCode = gaiaClient->open(file, idv);
  if (errorCode == 0) {
    if (showProgress) {
      cout << " found version " << gaiaClient->getFileVersion() << "." << endl;
    }
  } else {
    if (showProgress) {
      cout << " failed!" << endl;
    }
    if (jsonOutput) {
      printJsonResult(errorCode, errorCodeToMessage(statusCode));
    } else {
      cerr << "Failed to access " << idv.toUri() << ": " << errorCodeToMessage(statusCode) << endl;
    }
    return errorCode;
  }
  size_t totalSize = static_cast<size_t>(file->getTotalSize());
  string targetPath;
  if (downloadLocation.empty() || os::isDir(downloadLocation)) {
    string fileName = gaiaClient->getFileName();
    if (fileName.empty()) {
      cerr << "Failed to get file name for " << idv.toUri() << endl;
      fileName = "Gaia-recording-id-" + to_string(idv.id);
    }
    targetPath = os::pathJoin(downloadLocation, fileName);
  } else {
    targetPath = downloadLocation;
  }
  // If the file exists, and the file's size is what we expect, don't redownload
  if (os::getFileSize(targetPath) == file->getTotalSize()) {
    if (jsonOutput) {
      printJsonResult(
          statusCode, errorCodeToMessage(statusCode), {{kLocalPathResult, targetPath}}, idv.id);
    } else if (showProgress) {
      cout << idv.toUri() << " is already downloaded as " << targetPath << ". " << endl;
    }
    return statusCode;
  }
  os::remove(targetPath);
  string tmpDownloadPath = os::getUniquePath(targetPath);
  FILE* outfile = os::fileOpen(tmpDownloadPath, "wb");
  if (outfile) {
    vector<char> buffer(kDownloadChunkSize);
    for (size_t offset = 0; statusCode == 0 && offset < totalSize; offset += kDownloadChunkSize) {
      progress.show(kProgressMessage, offset, totalSize);
      size_t length = std::min<size_t>(kDownloadChunkSize, totalSize - offset);
      statusCode = file->read(buffer.data(), length);
      if (statusCode == 0 && os::fileWrite(buffer.data(), 1, length, outfile) != length) {
        statusCode = compoundError(errno, FAILURE);
      }
    }
    progress.show(kProgressMessage, totalSize, totalSize);
    statusCode = compoundError(statusCode, fflush(outfile));
    statusCode = compoundError(statusCode, os::fileClose(outfile));
  } else {
    statusCode = compoundError(errno, FAILURE);
  }
  progress.clear();
  if (statusCode == 0) {
    statusCode = os::rename(tmpDownloadPath, targetPath);
  } else {
    os::remove(tmpDownloadPath);
  }
  if (jsonOutput) {
    printJsonResult(
        statusCode, errorCodeToMessage(statusCode), {{kLocalPathResult, targetPath}}, idv.id);
  } else if (statusCode != 0) {
    cerr << "Download failed, error: " << errorCodeToMessage(statusCode) << endl;
  } else if (showProgress) {
    double duration = os::getTimestampSec() - timeBefore;
    cout << "Downloaded " << RecordFileInfo::humanReadableFileSize(totalSize) << " in " << fixed
         << setprecision(1) << duration << "s, at "
         << RecordFileInfo::humanReadableFileSize(totalSize / duration) << "/s, saved as '"
         << targetPath << "'." << endl;
  }
  return statusCode;
}

int verbatimInMemoryDownload(GaiaIdFileVersion idv, std::ostream& output, bool showProgress) {
  ProgressPrinter progress(showProgress);
  const char* kProgressMessage = "Downloading ";
  double timeBefore = os::getTimestampSec();
  int statusCode = 0;
  std::unique_ptr<GaiaClient> gaiaClient = GaiaClient::makeInstance();
  std::unique_ptr<FileHandler> file;
  if (showProgress) {
    cout << "Opening " << idv.toUri() << "...";
    cout.flush();
  }
  int errorCode = gaiaClient->open(file, idv);
  if (errorCode == 0) {
    if (showProgress) {
      cout << " found version " << gaiaClient->getFileVersion() << "." << endl;
    }
  } else {
    if (showProgress) {
      cout << " failed!" << endl;
    }
    cerr << "Failed to access " << idv.toUri() << ": " << errorCodeToMessage(errorCode) << endl;
    return errorCode;
  }
  size_t totalSize = static_cast<size_t>(file->getTotalSize());
  vector<char> buffer(std::min<size_t>(kDownloadChunkSize, totalSize));
  for (size_t offset = 0; offset < totalSize; offset += kDownloadChunkSize) {
    progress.show(kProgressMessage, offset, totalSize);
    size_t length = std::min<size_t>(kDownloadChunkSize, totalSize - offset);
    int error = file->read(buffer.data(), length);
    if (error == 0) {
      output.write(buffer.data(), static_cast<streamsize>(length));
    } else {
      statusCode = error;
      break;
    }
  }
  output.flush();
  progress.show(kProgressMessage, totalSize, totalSize);
  progress.clear();
  if (statusCode != 0) {
    cerr << "Download failed, error: " << errorCodeToMessage(statusCode) << endl;
  } else if (showProgress) {
    double duration = os::getTimestampSec() - timeBefore;
    cout << "Downloaded " << RecordFileInfo::humanReadableFileSize(totalSize) << " in " << fixed
         << setprecision(1) << duration << "s, at "
         << RecordFileInfo::humanReadableFileSize(totalSize / duration) << "/s" << endl;
  }
  return statusCode;
}

// Stream an open FileHandler, write data locally in chunks in a temp location,
// so we can upload the file unmodified to Gaia. Used for new uploads & updates.
static int downloadUpload(
    unique_ptr<FileHandler>& file,
    unique_ptr<UploadMetadata>& metadata,
    ProgressPrinter& progress,
    GaiaId& outGaiaId) {
  const char* kReading = "Reading ";
  string chunkedFilePath = os::getTempFolder() + "download.tmp";
  DiskFile chunkedFile;
  int status = chunkedFile.create(chunkedFilePath);
  if (status != 0) {
    cerr << "Can't create temp file at " << chunkedFilePath << ": " << errorCodeToMessage(status)
         << endl;
    return status;
  }
  unique_ptr<GaiaUploader> uploader_ =
      std::make_unique<GaiaUploader>(metadata->getUploadDestination());
  UploadId uploadId;
  status = uploader_->startChunkedFileUpload(move(metadata), uploadId, true);
  if (status != 0) {
    cerr << "Can't initiate upload: " << errorCodeToMessage(status) << endl;
    return status;
  }
  vector<char> buffer;
  const size_t fileSize = static_cast<size_t>(file->getTotalSize());
  const size_t kMaxChunkSize = GaiaClientConfig::getInstance().getUploadBlockSizeMB() * 1024 * 1024;
  // limit the number of chunks on disk: for each upload thread, we want 3, one being uploaded, and
  // up to two ready to upload next. So 3 chunks per upload thread.
  const size_t kMaxQueueSize =
      GaiaClientConfig::getInstance().getUploadThreadPoolSize() * kMaxChunkSize * 3;
  for (size_t chunkOffset = 0; chunkOffset < fileSize; chunkOffset += kMaxChunkSize) {
    // Download one disk chunk, one download chunk size at a time...
    size_t diskChunkSize = std::min<size_t>(kMaxChunkSize, fileSize - chunkOffset);
    for (size_t readOffset = chunkOffset; readOffset < chunkOffset + diskChunkSize;
         readOffset += kDownloadChunkSize) {
      size_t downloadChunkSize =
          std::min<size_t>(kDownloadChunkSize, chunkOffset + diskChunkSize - readOffset);
      progress.show(kReading, readOffset + downloadChunkSize / 8, fileSize);
      buffer.resize(downloadChunkSize);
      status = file->read(buffer.data(), buffer.size());
      if (status != 0) {
        cerr << "Failed to read source: " << errorCodeToMessage(status) << endl;
        return status;
      }
      progress.show(kReading, readOffset + downloadChunkSize / 2, fileSize);
      status = chunkedFile.write(buffer.data(), buffer.size());
      if (status != 0) {
        cerr << "Failed to write to temp file: " << errorCodeToMessage(status) << endl;
        return status;
      }
    }
    string chunkPath;
    size_t chunkIndex;
    if (!chunkedFile.getCurrentChunk(chunkPath, chunkIndex)) {
      cerr << "Can't get current chunk..." << endl;
      return FAILURE;
    }
    status = chunkedFile.addChunk();
    if (status != 0) {
      cerr << "Can't create new local chunk: " << errorCodeToMessage(status) << endl;
      return status;
    }
    bool isLastChunk = (chunkOffset + diskChunkSize == fileSize);
    status = uploader_->addChunk(
        uploadId, chunkPath, os::getFileSize(chunkPath), chunkIndex, isLastChunk);
    if (status != 0) {
      cerr << "Can't upload next chunk: " << errorCodeToMessage(status) << endl;
      return status;
    }
    while (uploader_->getQueueSize() > kMaxQueueSize) {
      size_t uploadedSize = chunkOffset + diskChunkSize - uploader_->getQueueSize();
      progress.show("Uploading ", uploadedSize, fileSize);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  while (uploader_->getQueueSize() > kMaxChunkSize) {
    size_t uploadedSize = fileSize - uploader_->getQueueSize();
    progress.show("Finishing upload ", uploadedSize, fileSize);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  progress.show("Finishing upload...");
  status = uploader_->finishUpload(uploadId, outGaiaId);
  progress.clear();
  return status;
}

static int localFileUpload(
    const string& path,
    unique_ptr<UploadMetadata>& metadata,
    bool showProgress,
    GaiaId& outGaiaId) {
  GaiaUploader uploader(metadata->getUploadDestination());
  UploadId uploadId;
  int status = uploader.upload(move(metadata), path, uploadId);
  if (status != 0) {
    cerr << "Failed to initiate Gaia upload." << endl;
    return status;
  }
  size_t totalSize = uploader.getQueueSize();
  if (showProgress) {
    size_t leftSize;
    while (uploader.getUploadStatus(uploadId) == GaiaUploadStatus::InProgress &&
           (leftSize = uploader.getQueueSize()) > 0) {
      cout << kResetCurrentLine << "Uploading "
           << RecordFileInfo::humanReadableFileSize(totalSize - leftSize) << " / "
           << RecordFileInfo::humanReadableFileSize(totalSize) << "...";
      cout.flush();
      const std::chrono::milliseconds kUpdateDelay(250);
      std::this_thread::sleep_for(kUpdateDelay);
    }
    cout << kResetCurrentLine << "Finishing-up...";
    cout.flush();
  }
  status = uploader.finishUpload(uploadId, outGaiaId);
  if (showProgress) {
    cout << kResetCurrentLine;
  }
  return status;
}

int verbatimUpload(
    const string& path,
    unique_ptr<UploadMetadata>& metadata,
    GaiaId& outGaiaId,
    bool showProgress,
    bool jsonOutput) {
  ProgressPrinter progress(showProgress);
  outGaiaId = 0;
  double timeBefore = os::getTimestampSec();
  int status = 0;
  GaiaId gaiaId;
  int64_t fileSize = os::getFileSize(path);
  if (fileSize >= 0) {
    status = localFileUpload(path, metadata, showProgress, gaiaId);
  } else {
    unique_ptr<FileHandler> file;
    if ((status = FileHandlerFactory::getInstance().delegateOpen(path, file)) != 0) {
      if (jsonOutput) {
        printJsonResult(status, errorCodeToMessage(status));
      } else {
        cerr << "Failed to open '" << path << "': " << errorCodeToMessage(status) << endl;
      }
      return status;
    }
    fileSize = file->getTotalSize();
    status = downloadUpload(file, metadata, progress, gaiaId);
  }
  if (jsonOutput) {
    printJsonResult(status, errorCodeToMessage(status), {}, gaiaId);
  } else if (status != 0) {
    cerr << "Upload to Gaia failed, error: " << errorCodeToMessage(status) << "." << endl;
  } else {
    outGaiaId = gaiaId;
    double duration = os::getTimestampSec() - timeBefore;
    cout << "Uploaded " << RecordFileInfo::humanReadableFileSize(fileSize) << " in " << fixed
         << setprecision(1) << duration << "s, at "
         << RecordFileInfo::humanReadableFileSize(fileSize / duration) << "/s, Gaia ID: " << gaiaId
         << endl;
  }
  return status;
}

int verbatimUpdate(
    GaiaId updateId,
    FilteredVRSFileReader& source,
    bool showProgress,
    bool jsonOutput) {
  ProgressPrinter progress(showProgress);
  double timeBefore = os::getTimestampSec();
  unique_ptr<UploadMetadata> uploadMetadata = make_unique<UploadMetadata>();
  uploadMetadata->setUploadType(UploadType::Update);
  uploadMetadata->setUpdateId(updateId);
  uploadMetadata->setFileName(source.getFileName());
  int64_t fileSize = source.isUsingGaiaId ? -1 : os::getFileSize(source.path);
  int status;
  GaiaId gaiaId;
  if (fileSize >= 0) {
    status = localFileUpload(source.path, uploadMetadata, showProgress, gaiaId);
  } else {
    unique_ptr<FileHandler> file;
    string path = source.getPathOrUri();
    if ((status = FileHandlerFactory::getInstance().delegateOpen(path, file)) != 0) {
      if (jsonOutput) {
        printJsonResult(status, errorCodeToMessage(status));
      } else {
        cerr << "Failed to open '" << path << "': " << errorCodeToMessage(status) << endl;
      }
      return status;
    }
    fileSize = file->getTotalSize();
    status = downloadUpload(file, uploadMetadata, progress, gaiaId);
  }
  GaiaClient::makeInstance()->clearCachedLookup(updateId);
  if (jsonOutput) {
    printJsonResult(status, errorCodeToMessage(status), {}, updateId);
  } else if (status != 0) {
    cerr << "Update of gaia:" << updateId << " failed: " << errorCodeToMessage(status) << endl;
  } else {
    double duration = os::getTimestampSec() - timeBefore;
    cout << "Uploaded " << RecordFileInfo::humanReadableFileSize(fileSize) << " in " << fixed
         << setprecision(1) << duration << "s, at "
         << RecordFileInfo::humanReadableFileSize(fileSize / duration) << "/s." << endl;
    cout << "Update of gaia:" << updateId << " complete." << endl;
  }
  return status;
}

int cacheDownload(GaiaIdFileVersion idv, bool showProgress, bool jsonOutput) {
  std::optional<string> cachePath = CachedGaiaFileHandler::getCachePath(idv);
  os::makeDirectories(os::getParentFolder(*cachePath));
  return cachePath ? verbatimDownload(idv, *cachePath, showProgress, jsonOutput) : FAILURE;
}

void uncacheDownload(GaiaIdFileVersion idv) {
  std::optional<string> cachePath = CachedGaiaFileHandler::getCachePath(idv);
  if (cachePath) {
    os::remove(*cachePath);
  }
}

string jsonResult(
    int status,
    const string& failureMessage,
    const map<string, string>& successFields,
    GaiaId gaiaId) {
  using namespace fb_rapidjson;
  JDocument document;
  document.SetObject();
  JsonWrapper wrapper{document, document.GetAllocator()};
  wrapper.addMember("status", status);
  if (status != 0) {
    if (!failureMessage.empty()) {
      wrapper.addMember("message", failureMessage);
    }
  } else {
    for (auto& extra : successFields) {
      wrapper.addMember(extra.first, extra.second);
    }
  }
  if (gaiaId != 0) {
    wrapper.addMember(kGaiaIdResult, static_cast<uint64_t>(gaiaId));
  }
  return jDocumentToJsonString(document);
}

int printJsonResult(
    int status,
    const string& failureMessage,
    const map<string, string>& successFields,
    GaiaId gaiaId) {
  cout << jsonResult(status, failureMessage, successFields, gaiaId) << endl;
  return status;
}

} // namespace vrs::utils
