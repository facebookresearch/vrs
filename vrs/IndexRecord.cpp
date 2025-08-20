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

#include "IndexRecord.h"

#include <algorithm>
#include <array>
#include <map>

#define DEFAULT_LOG_CHANNEL "VRSIndexRecord"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/os/CompilerAttributes.h>
#include <vrs/os/Time.h>

#include "Compressor.h"
#include "Decompressor.h"
#include "ErrorCode.h"
#include "FileHandler.h"
#include "ProgressLogger.h"

namespace {
using namespace std;
using namespace vrs;
using namespace vrs::IndexRecord;

const uint32_t kMaxBatchSize = 100000;

// Maximum number of records in a single index record. To avoid a potentially corrupt file that
// requests too much memory, we limit the maximum record count to this arbitrarily large number.
constexpr size_t kMaxRecordCount = 500000000;

// Compression presets, in increasingly tighter settings, starting with NONE, which will be only
// used when there are too few index entries for compression to reasonably work...
#if IS_MOBILE_PLATFORM()
CompressionPreset kDefaultCompression = CompressionPreset::ZstdLight;
const array<CompressionPreset, 3> kCompressionLevels = {
    CompressionPreset::None,
    CompressionPreset::ZstdMedium,
    CompressionPreset::ZstdHigh};
#else // !IS_MOBILE_PLATFORM()
CompressionPreset kDefaultCompression = CompressionPreset::ZstdMedium;
const array<CompressionPreset, 3> kCompressionLevels = {
    CompressionPreset::None,
    CompressionPreset::ZstdHigh,
    CompressionPreset::ZstdTight};

#endif // !IS_MOBILE_PLATFORM()

// Compression doesn't work for small sizes, under this number of records, don't try to compress
// If we don't have enough records, start with no compression (also for preallocation)
size_t firstCompressionPresetIndex(size_t recordCount) {
  static const size_t kMinCompressionIndexSize = 100;
  return recordCount < kMinCompressionIndexSize ? 0 : 1;
}

// When tuning compression, this logging is very useful, so let's keep it in the code.
const bool kLogStats = false;

/// Format of the index record:
///
/// kClassicIndexFormatVersion:
///   uint32_t recordableCount; //count of StreamId structs, always present, value may be 0.
///   DiskStreamId streamId[streamCount]; // one per Recordable instance
///   uint32_t recordCount; // count of DiskRecordInfo structs, always present, value may be 0.
///   DiskRecordInfo recordInfo[recordCount]; // one per actual record
///
/// kSplitIndexFormatVersion:
///   DiskRecordInfo recordInfo[recordCount]; // one per actual record.
///   A split index record may not have a valid size, if the recording was interrupted.
///   In that case, you must look for the end of the first chunk at most, then try to extend the
///   index after the last record found in the index.
///   The first user record's offset might also be missing: the first user record should start at
///   the second file chunk's first byte.

int writeDiskInfos(
    WriteFileHandler& file,
    const deque<DiskRecordInfo>& records,
    uint32_t& writtenSize,
    Compressor& compressor,
    CompressionPreset preset = CompressionPreset::None,
    size_t maxWriteSize = 0) {
  map<RecordSignature, uint32_t> recordCounter;
  // Write the count of records, and one RecordIndexStruct for each (in batch of fixed size)
  uint32_t recordsLeft = static_cast<uint32_t>(records.size());
  vector<DiskRecordInfo> recordStructs;
  deque<DiskRecordInfo>::const_iterator iter = records.begin();
  if (preset != CompressionPreset::None) {
    IF_ERROR_RETURN(
        compressor.startFrame(recordsLeft * sizeof(DiskRecordInfo), preset, writtenSize));
  }
  while (recordsLeft > 0) {
    uint32_t batchSize = (recordsLeft < kMaxBatchSize) ? recordsLeft : kMaxBatchSize;
    recordStructs.resize(0);
    recordStructs.reserve(batchSize);
    for (uint32_t k = 0; k < batchSize; ++k, ++iter) {
      recordStructs.emplace_back(*iter);
      recordCounter[RecordSignature{iter->streamId.getStreamId(), iter->getRecordType()}]++;
    }
    size_t writeSize = sizeof(DiskRecordInfo) * batchSize;
    if (preset != CompressionPreset::None) {
      IF_ERROR_RETURN(compressor.addFrameData(
          file, recordStructs.data(), writeSize, writtenSize, maxWriteSize));
    } else {
      if (maxWriteSize > 0 && writeSize > maxWriteSize) {
        return TOO_MUCH_DATA;
      }
      WRITE_OR_LOG_AND_RETURN(file, recordStructs.data(), writeSize);
      writtenSize += static_cast<uint32_t>(writeSize);
    }
    recordsLeft -= batchSize;
  }
  for (MAYBE_UNUSED auto counter : recordCounter) {
    XR_LOGD(
        "  {}: {} {} {}",
        counter.first.streamId.getName(),
        counter.second,
        toString(counter.first.recordType),
        (counter.second > 1 ? " records." : " record."));
  }
  if (preset != CompressionPreset::None) {
    IF_ERROR_RETURN(compressor.endFrame(file, writtenSize, maxWriteSize));
  }
  return 0;
}

int writeClassicIndexRecord(
    WriteFileHandler& file,
    const set<StreamId>& streamIds,
    const deque<DiskRecordInfo>& records,
    uint32_t& outLastRecordSize,
    Compressor& compressor,
    CompressionPreset preset = CompressionPreset::None,
    uint32_t preallocatedByteSize = 0) {
  int64_t indexRecordOffset = file.getPos();
  FileFormat::RecordHeader indexRecordHeader;
  uint32_t preludeSize = static_cast<uint32_t>(
      sizeof(uint32_t) + streamIds.size() * sizeof(DiskStreamId) + sizeof(uint32_t));
  if (preallocatedByteSize > 0 && preallocatedByteSize < preludeSize) {
    return TOO_MUCH_DATA;
  }
  uint32_t uncompressedSize =
      preludeSize + static_cast<uint32_t>(records.size() * sizeof(DiskRecordInfo));
  indexRecordHeader.initIndexHeader(
      kClassicIndexFormatVersion, uncompressedSize, outLastRecordSize, CompressionType::None);
  // If the record was pre-allocated, then that's its actual size.
  if (preallocatedByteSize > 0) {
    indexRecordHeader.recordSize.set(static_cast<uint32_t>(preallocatedByteSize));
  }
  // Write the index record a first time. When compressing, we don't know the actual size until
  // after we wrote it, so we will need to rewrite it... :-(
  WRITE_OR_LOG_AND_RETURN(file, &indexRecordHeader, sizeof(indexRecordHeader));

  // Write the count of streams, and one DiskStreamId struct for each
  FileFormat::LittleEndian<uint32_t> recordableCount{static_cast<uint32_t>(streamIds.size())};
  WRITE_OR_LOG_AND_RETURN(file, &recordableCount, sizeof(recordableCount));
  vector<DiskStreamId> diskStreams;
  diskStreams.reserve(streamIds.size());
  for (StreamId id : streamIds) {
    diskStreams.emplace_back(id);
  }
  size_t writeSize = sizeof(DiskStreamId) * streamIds.size();
  WRITE_OR_LOG_AND_RETURN(file, diskStreams.data(), writeSize);
  diskStreams.clear();

  FileFormat::LittleEndian<uint32_t> recordCount{static_cast<uint32_t>(records.size())};
  WRITE_OR_LOG_AND_RETURN(file, &recordCount, sizeof(recordCount));

  uint32_t writtenBytes = 0;
  if (preallocatedByteSize == 0) {
    IF_ERROR_LOG_AND_RETURN(writeDiskInfos(file, records, writtenBytes, compressor, preset));
  } else {
    size_t maxWriteSize = preallocatedByteSize - sizeof(indexRecordHeader) - preludeSize;
    int status = writeDiskInfos(file, records, writtenBytes, compressor, preset, maxWriteSize);
    if (status != SUCCESS) {
      // TOO_MUCH_DATA is not even worth a warning, but other errors are a real problem!
      if (status != TOO_MUCH_DATA) {
        XR_LOGE("writeDiskInfos failed: {}, {}", status, errorCodeToMessage(status));
      }
      return status;
    }
  }

  uint32_t thisSize = static_cast<uint32_t>(preludeSize + writtenBytes);
  // if compressing, we need to rewrite the index record's header with the proper sizes
  if (preset != CompressionPreset::None) {
    int64_t nextRecordOffset = file.getPos();
    indexRecordHeader.initIndexHeader(
        kClassicIndexFormatVersion, thisSize, outLastRecordSize, CompressionType::Zstd);
    // If the record was pre-allocated, that's its actual size, even if we don't use it all.
    if (preallocatedByteSize > 0) {
      indexRecordHeader.recordSize.set(static_cast<uint32_t>(preallocatedByteSize));
      if (kLogStats) {
        MAYBE_UNUSED size_t usablePreallocatedSize =
            preallocatedByteSize - sizeof(indexRecordHeader);
        XR_LOGI(
            "Pre-allocated index worked. Using {} bytes out of {} instead of {}, or "
            "{:.2f}% of allocation, {:.2f}% allocated, {:.2f}% used.",
            thisSize,
            usablePreallocatedSize,
            uncompressedSize,
            thisSize * 100.f / usablePreallocatedSize,
            usablePreallocatedSize * 100.f / uncompressedSize,
            thisSize * 100.f / uncompressedSize);
      }
    }
    indexRecordHeader.uncompressedSize.set(uncompressedSize);
    IF_ERROR_LOG_AND_RETURN(file.setPos(indexRecordOffset));
    IF_ERROR_LOG_AND_RETURN(file.overwrite(indexRecordHeader));
    IF_ERROR_LOG_AND_RETURN(file.setPos(nextRecordOffset));
  }
  outLastRecordSize = indexRecordHeader.recordSize.get();
  return 0;
}

inline bool isValid(Record::Type recordType) {
  return enumIsValid<Record::Type>(recordType);
}

} // namespace

namespace vrs {

IndexRecord::Reader::Reader(
    FileHandler& file,
    FileFormat::FileHeader& fileHeader,
    ProgressLogger* progressLogger,
    set<StreamId>& outStreamIds,
    vector<IndexRecord::RecordInfo>& outIndex)
    : file_{file},
      totalFileSize_{file_.getTotalSize()},
      fileHeader_{fileHeader},
      progressLogger_{progressLogger},
      streamIds_{outStreamIds},
      index_{outIndex} {}

int IndexRecord::Reader::readRecord(int64_t firstUserRecordOffset, int64_t& outUsedFileSize) {
  streamIds_.clear();
  index_.clear();
  diskIndex_.reset();
  indexComplete_ = false;
  hasSplitHeadChunk_ = false;
  sortErrorCount_ = 0;
  droppedRecordCount_ = 0;
  int error =
      readRecord(fileHeader_.indexRecordOffset.get(), firstUserRecordOffset, outUsedFileSize);
  if (error == 0) {
    if (sortErrorCount_ > 0) {
      XR_LOGW("{} record(s) not sorted properly. Sorting index.", sortErrorCount_);
      sort(index_.begin(), index_.end());
    }
    if (droppedRecordCount_ > 0) {
      XR_LOGW("{} records are beyond the end of the file. Dropping them.", droppedRecordCount_);
    }
  }
  return error;
}

int IndexRecord::Reader::readRecord(
    int64_t indexRecordOffset,
    int64_t firstUserRecordOffset,
    int64_t& outUsedFileSize) {
  if (indexRecordOffset == 0) {
    XR_LOGW(
        "VRS file has no index. Was the recording interrupted by a crash or lack of disk space?");
    return INDEX_RECORD_ERROR;
  }
  int error = file_.setPos(indexRecordOffset);
  if (error != 0) {
    XR_LOGW("Seek to index record failed: {}", errorCodeToMessage(error));
    return INDEX_RECORD_ERROR;
  }
  // maybe headers are larger now: allocate a possibly larger buffer than FileFormat::RecordHeader
  uint32_t recordHeaderSize = fileHeader_.recordHeaderSize.get();
  vector<uint8_t> headerBuffer(recordHeaderSize);
  FileFormat::RecordHeader* recordHeader =
      reinterpret_cast<FileFormat::RecordHeader*>(headerBuffer.data());
  if (file_.read(recordHeader, recordHeaderSize) != 0) {
    if (file_.getLastRWSize() == 0 && file_.isEof()) {
      XR_LOGW("Reading index failed: End of file.");
      return INDEX_RECORD_ERROR;
    }
    XR_LOGW(
        "Can't read index header. Read {} bytes, expected {} bytes.",
        file_.getLastRWSize(),
        recordHeaderSize);
    return file_.getLastError();
  }
  if (recordHeader->recordSize.get() < recordHeaderSize) {
    XR_LOGE("Record size too small. Corrupt?");
    return INDEX_RECORD_ERROR;
  }
  if (!recordHeader->isSanityCheckOk()) {
    XR_LOGE("Record header sanity check failed. Corrupt?");
    return INDEX_RECORD_ERROR;
  }
  size_t indexByteSize = recordHeader->recordSize.get() - static_cast<uint32_t>(recordHeaderSize);
  if (recordHeader->formatVersion.get() == kClassicIndexFormatVersion) {
    return readClassicIndexRecord(
        indexByteSize,
        recordHeader->uncompressedSize.get(),
        firstUserRecordOffset,
        outUsedFileSize);
  } else if (recordHeader->formatVersion.get() == kSplitIndexFormatVersion) {
    return readSplitIndexRecord(
        indexByteSize, recordHeader->uncompressedSize.get(), outUsedFileSize);
  }
  XR_LOGW("Unsupported index format.");
  return UNSUPPORTED_INDEX_FORMAT_VERSION;
}

int IndexRecord::Reader::readClassicIndexRecord(
    size_t indexRecordPayloadSize,
    size_t uncompressedSize,
    int64_t firstUserRecordOffset,
    int64_t& outUsedFileSize) {
  const size_t kCountersCount = 2;
  if (indexRecordPayloadSize < sizeof(uint32_t) * kCountersCount) {
    XR_LOGE("Index record way too small. Corrupt file or index?");
    return INDEX_RECORD_ERROR;
  }
  size_t preludeSize = sizeof(uint32_t) * kCountersCount; // discount counters
  FileFormat::LittleEndian<uint32_t> typeCountRaw;
  if (file_.read(typeCountRaw) != 0) {
    return file_.getLastError();
  }
  uint32_t typeCount = typeCountRaw.get();
  if (typeCount > 0) {
    const size_t readSize = sizeof(DiskStreamId) * typeCount;
    if (readSize > indexRecordPayloadSize - preludeSize) {
      XR_LOGE("Index record too small. Corrupt file or index?");
      return INDEX_RECORD_ERROR;
    }
    vector<DiskStreamId> diskStreams;
    diskStreams.resize(typeCount);
    if (file_.read(diskStreams.data(), readSize) != 0) {
      return file_.getLastError();
    }
    preludeSize += static_cast<uint32_t>(readSize);
    for (auto diskStruct : diskStreams) {
      streamIds_.insert(StreamId{diskStruct.getTypeId(), diskStruct.getInstanceId()});
    }
  }
  FileFormat::LittleEndian<uint32_t> recordCountRaw;
  if (file_.read(recordCountRaw) != 0) {
    return file_.getLastError();
  }
  uint32_t recordCount = recordCountRaw.get();
  const size_t indexSize = indexRecordPayloadSize - preludeSize;
  if (recordCount > 0) {
    if (recordCount > kMaxRecordCount) {
      XR_LOGE("Too many records in index ({} > {}). Corrupt index?", recordCount, kMaxRecordCount);
      return INDEX_RECORD_ERROR;
    }
    vector<DiskRecordInfo> recordStructs(recordCount);
    int status = 0;
    if (uncompressedSize > 0) {
      Decompressor decompressor;
      size_t frameSize = 0;
      size_t maxReadSize = indexSize;
      status = decompressor.initFrame(file_, frameSize, maxReadSize);
      if (status == 0) {
        if (frameSize != sizeof(DiskRecordInfo) * recordCount) {
          XR_LOGE("Compressed index size unexpected. Corrupt index?");
          return INDEX_RECORD_ERROR;
        }
        status = decompressor.readFrame(file_, recordStructs.data(), frameSize, maxReadSize);
      }
    } else {
      if (sizeof(DiskRecordInfo) * recordCount > indexSize) {
        XR_LOGE("More records expected than can fit in the index record. Corrupt index?");
        return INDEX_RECORD_ERROR;
      }
      status = readDiskInfo(recordStructs);
    }
    if (status != 0) {
      XR_LOGW("Failed to read entire index.");
      return status;
    }
    index_.reserve(recordStructs.size());
    int64_t fileOffset = firstUserRecordOffset;
    for (auto record : recordStructs) {
      if (!isValid(record.getRecordType())) {
        XR_LOGE(
            "Unexpected index record entry: Stream Id: {} Type: {} Size: {} Timestamp: {}",
            record.getStreamId().getNumericName(),
            toString(record.getRecordType()),
            record.recordSize.get(),
            record.timestamp.get());
        return INDEX_RECORD_ERROR;
      }
      int64_t nextFileOffset = fileOffset + record.recordSize.get();
      if (nextFileOffset > totalFileSize_) {
        droppedRecordCount_ = static_cast<int32_t>(recordStructs.size() - index_.size());
        break; // The file is too short, and this record goes beyond the end...
      }
      index_.emplace_back(
          record.timestamp.get(), fileOffset, record.getStreamId(), record.getRecordType());
      if (index_.size() > 1 && index_.back() < index_[index_.size() - 2]) {
        sortErrorCount_++;
      }
      fileOffset = nextFileOffset;
    }
    outUsedFileSize = fileOffset;
  }
  indexComplete_ = true;
  // we're just past the index record, which might be the end of the file
  int64_t offsetPastIndexRecord = file_.getPos();
  if (offsetPastIndexRecord > outUsedFileSize) {
    outUsedFileSize = offsetPastIndexRecord;
  }
  return 0;
}

#define BREAK_ON_ERROR(operation__)                                               \
  if ((error = (operation__)) != 0) {                                             \
    XR_LOGE("{} failed: {}, {}", #operation__, error, errorCodeToMessage(error)); \
    break;                                                                        \
  }                                                                               \
  (0) // Empty statement to require a semicolon after the macro, and silence a compiler warning

#define BREAK_ON_FALSE(check__)      \
  if (!(check__)) {                  \
    XR_LOGE("{} failed.", #check__); \
    break;                           \
  }                                  \
  (0) // Empty statement to require a semicolon after the macro, and silence a compiler warning

int IndexRecord::Reader::readSplitIndexRecord(
    size_t indexByteSize,
    size_t uncompressedSize,
    int64_t& outUsedFileSize) {
  // The index record's size is only updated after the index body is fully written,
  // because we will add to the index while the file is written
  int64_t firstUserRecordOffset = fileHeader_.firstUserRecordOffset.get();
  bool noRecords = (firstUserRecordOffset == totalFileSize_);
  int64_t currentPos = file_.getPos();
  int64_t chunkStart{}, chunkSize{};
  if (!XR_VERIFY(file_.getChunkRange(chunkStart, chunkSize) == 0) || !XR_VERIFY(chunkSize > 0) ||
      !XR_VERIFY(
          (currentPos >= chunkStart && currentPos < chunkStart + chunkSize) ||
          currentPos == totalFileSize_ && noRecords)) {
    return INDEX_RECORD_ERROR;
  }
  const int64_t nextChunkStart = chunkStart + chunkSize;
  indexComplete_ = ((indexByteSize > 0 || noRecords) && firstUserRecordOffset > 0);
  if (chunkStart == 0) {
    const size_t chunkLeft = static_cast<size_t>(nextChunkStart - currentPos);
    if (indexByteSize == 0) {
      if (nextChunkStart == totalFileSize_ && firstUserRecordOffset == 0) {
        // There is a single chunk, we don't know the size of the index record,
        // nor where the first user record is: we must give up! :-(
        XR_LOGE("VRS file not recoverable: can't determine where the user records are.");
        return INDEX_RECORD_ERROR;
      }
      indexByteSize = chunkLeft;
    } else if (chunkLeft < indexByteSize) {
      XR_LOGW("Index record too short. {} bytes missing...", indexByteSize - chunkLeft);
      indexByteSize = chunkLeft;
      indexComplete_ = false;
    }
    hasSplitHeadChunk_ = nextChunkStart < totalFileSize_;
    if (firstUserRecordOffset == 0) {
      firstUserRecordOffset = nextChunkStart;
    } else if (nextChunkStart < firstUserRecordOffset) {
      XR_LOGW(
          "Index record too short to reach the first user record. {} bytes missing...",
          firstUserRecordOffset - nextChunkStart);
      indexComplete_ = false;
      firstUserRecordOffset = nextChunkStart;
    }
  } else {
    // We're already at the next chunk! there is no data in the index!
    indexByteSize = 0;
    indexComplete_ = false;
    hasSplitHeadChunk_ = chunkStart < totalFileSize_;
    firstUserRecordOffset = currentPos;
  }
  outUsedFileSize = firstUserRecordOffset;
  size_t sizeToRead = (uncompressedSize == 0) ? indexByteSize : uncompressedSize;
  const size_t extraBytes = sizeToRead % sizeof(IndexRecord::DiskRecordInfo);
  if (extraBytes > 0) {
    XR_LOGW("The index record has {} extra bytes that we will ignore.", extraBytes);
    sizeToRead -= extraBytes;
    indexComplete_ = false;
  }
  const size_t maxRecordInfoCount = sizeToRead / sizeof(IndexRecord::DiskRecordInfo);
  if (maxRecordInfoCount == 0) {
    if (!noRecords) {
      XR_LOGW("No index data to read.");
    }
    return 0;
  } else if (maxRecordInfoCount > kMaxRecordCount) {
    XR_LOGE(
        "Too many records in index ({} > {}). Corrupt index?", maxRecordInfoCount, kMaxRecordCount);
    return INDEX_RECORD_ERROR;
  }
  vector<DiskRecordInfo> recordStructs(maxRecordInfoCount);
  if (uncompressedSize == 0) { // not compressed
    int status = readDiskInfo(recordStructs);
    if (status != 0) {
      XR_LOGW("Failed to read uncompressed index.");
      return status;
    }
  } else {
    size_t decompressedRecords = 0;
    Decompressor decompressor;
    int error = 0;
    char* endBuffer = reinterpret_cast<char*>(recordStructs.data()) + sizeToRead;
    while (sizeToRead > 0) {
      size_t frameSize = 0;
      BREAK_ON_ERROR(decompressor.initFrame(file_, frameSize, indexByteSize));
      BREAK_ON_FALSE(frameSize <= sizeToRead);
      BREAK_ON_ERROR(
          decompressor.readFrame(file_, endBuffer - sizeToRead, frameSize, indexByteSize));
      sizeToRead -= frameSize;
      decompressedRecords += frameSize / sizeof(DiskRecordInfo);
    }
    if (decompressedRecords < maxRecordInfoCount) {
      XR_LOGW(
          "Failed to read {} out of {} compressed index records.",
          (maxRecordInfoCount - decompressedRecords),
          maxRecordInfoCount);
      indexComplete_ = false;
      recordStructs.resize(decompressedRecords);
    }
  }
  index_.reserve(recordStructs.size());
  const uint32_t recordHeaderSize = fileHeader_.recordHeaderSize.get();
  for (const DiskRecordInfo& record : recordStructs) {
    Record::Type recordType = record.getRecordType();
    if (record.recordSize.get() < recordHeaderSize || !isValid(recordType)) {
      XR_LOGE(
          "Unexpected index record entry: Stream Id: {} Type: {} Size: {} Timestamp: {}",
          record.getStreamId().getNumericName(),
          toString(recordType),
          record.recordSize.get(),
          record.timestamp.get());
      return INDEX_RECORD_ERROR;
    }
    int64_t followingRecordOffset = outUsedFileSize + record.recordSize.get();
    if (droppedRecordCount_ > 0 || followingRecordOffset > totalFileSize_) {
      droppedRecordCount_++;
    } else {
      double timestamp = record.timestamp.get();
      StreamId streamId = record.getStreamId();
      index_.emplace_back(timestamp, outUsedFileSize, streamId, recordType);
      if (index_.size() > 1 && index_.back() < index_[index_.size() - 2]) {
        sortErrorCount_++;
      }
      if (diskIndex_) {
        diskIndex_->emplace_back(timestamp, record.recordSize.get(), streamId, recordType);
      }
      streamIds_.insert(streamId);
      outUsedFileSize = followingRecordOffset;
    }
  }
  return 0;
}

int IndexRecord::Reader::readDiskInfo(vector<DiskRecordInfo>& outRecords) {
  const size_t totalSize = sizeof(DiskRecordInfo) * outRecords.size();
  const size_t kMaxChunkSize = 8 * 1024 * 1024;
  size_t completedSize = 0;
  while (completedSize < totalSize) {
    const size_t chunkSize = min(kMaxChunkSize, totalSize - completedSize);
    if (file_.read(reinterpret_cast<char*>(outRecords.data()) + completedSize, chunkSize) != 0) {
      XR_LOGW("Failed to read entire index.");
      return file_.getLastError();
    }
    completedSize += chunkSize;
    if (!progressLogger_->logProgress("Reading index", completedSize, totalSize)) {
      return OPERATION_CANCELLED;
    }
  }
  if (!progressLogger_->logStatus("Reading index", 0)) {
    return OPERATION_CANCELLED;
  }
  return 0;
}

int IndexRecord::Reader::rebuildIndex(bool writeFixedIndex) {
  WriteFileHandler* writeFile = writeFixedIndex ? dynamic_cast<WriteFileHandler*>(&file_) : nullptr;
  if (writeFixedIndex && (writeFile == nullptr || !writeFile->reopenForUpdatesSupported())) {
    XR_LOGW("File modifications not supported by {}.", file_.getFileHandlerName());
    writeFixedIndex = false;
  }
  MAYBE_UNUSED double beforeIndexing = os::getTimestampSec();
  if (!file_.isOpened()) {
    XR_LOGE("No file open");
    return NO_FILE_OPEN;
  }
  size_t fileHeaderSize = fileHeader_.fileHeaderSize.get();
  if (fileHeaderSize < sizeof(FileFormat::FileHeader)) {
    XR_LOGE("Reindexing: File header too small");
    return REINDEXING_ERROR;
  }
  size_t recordHeaderSize = fileHeader_.recordHeaderSize.get();
  if (recordHeaderSize < sizeof(FileFormat::RecordHeader)) {
    XR_LOGE("Reindexing: Record header too small");
    return REINDEXING_ERROR;
  }
  // go to the first record header, just past the file header
  int64_t absolutePosition = static_cast<int64_t>(fileHeaderSize);
  if (file_.setPos(absolutePosition) != 0) {
    XR_LOGE(
        "Reindexing: Can't jump to offset {}. Error: {}",
        fileHeaderSize,
        errorCodeToMessage(file_.getLastError()));
    return file_.getLastError();
  }
  if (hasSplitHeadChunk_) {
    // go to the start of the second chunk
    int64_t chunkStart{}, chunkSize{};
    if (!XR_VERIFY(file_.getChunkRange(chunkStart, chunkSize) == 0) || !XR_VERIFY(chunkSize > 0) ||
        !XR_VERIFY(chunkStart == 0)) {
      return REINDEXING_ERROR;
    }
    absolutePosition = chunkSize;
    if (file_.setPos(absolutePosition) != 0) {
      XR_LOGE(
          "Reindexing: Can't jump to offset {}. Error: {}",
          chunkSize,
          errorCodeToMessage(file_.getLastError()));
      return file_.getLastError();
    }
  }
  streamIds_.clear();
  index_.clear();
  sortErrorCount_ = 0;
  diskIndex_ = writeFixedIndex ? make_unique<deque<IndexRecord::DiskRecordInfo>>() : nullptr;
  const size_t kFirstAllocation = 10000; // arbitrary start
  index_.reserve(kFirstAllocation);
  // maybe headers are larger now: allocate a possibly larger buffer than FileFormat::RecordHeader
  vector<uint8_t> headerBuffer(recordHeaderSize);
  FileFormat::RecordHeader* recordHeader =
      reinterpret_cast<FileFormat::RecordHeader*>(headerBuffer.data());
  uint32_t previousRecordSize = 0;
  bool distrustLastRecord = false;
  int error = 0;
  do {
    if (file_.read(recordHeader, recordHeaderSize) != 0) {
      if (file_.getLastRWSize() == 0 && file_.isEof()) {
        XR_LOGI("Reindexing: record #{} End of file.", index_.size());
        break;
      }
      XR_LOGW(
          "Reindexing: record #{}. Can't read record header. Read {} bytes, expected {} bytes.",
          index_.size(),
          file_.getLastRWSize(),
          previousRecordSize);
      error = REINDEXING_ERROR;
      break;
    }
    uint32_t headerPreviousRecordSize = recordHeader->previousRecordSize.get();
    if (headerPreviousRecordSize != previousRecordSize && !(hasSplitHeadChunk_ && index_.empty())) {
      XR_LOGW(
          "Reindexing: record #{}. Previous record size is {}, expected {}.",
          index_.size(),
          headerPreviousRecordSize,
          previousRecordSize);
      distrustLastRecord = true;
      error = REINDEXING_ERROR;
      break;
    }
    uint32_t recordSize = recordHeader->recordSize.get();
    if (recordSize < recordHeaderSize) {
      XR_LOGW(
          "Reindexing: record #{} too small. {} bytes, expected at least {} bytes.",
          index_.size(),
          recordSize,
          recordHeaderSize);
      distrustLastRecord = true;
      error = REINDEXING_ERROR;
      break;
    }
    if (!recordHeader->isSanityCheckOk()) {
      XR_LOGW("Reindexing: record #{} header sanity check failed.", index_.size());
      error = REINDEXING_ERROR;
      break;
    }
    RecordableTypeId recordableTypeId = recordHeader->getRecordableTypeId();
    uint32_t dataSize = recordSize - static_cast<uint32_t>(recordHeaderSize);
    if (recordableTypeId == RecordableTypeId::VRSIndex &&
        recordHeader->formatVersion.get() == kSplitIndexFormatVersion) {
      int64_t fileSizeUsed = 0;
      readSplitIndexRecord(0, recordHeader->uncompressedSize.get(), fileSizeUsed);
      if (!index_.empty()) {
        XR_LOGW("Found {} records in the split index.", index_.size());
        // we can skip all the records found in the index
        absolutePosition = fileSizeUsed;
        previousRecordSize = static_cast<uint32_t>(absolutePosition - index_.back().fileOffset);
      } else {
        // reading the split index failed: reindex from scratch
        streamIds_.clear();
        index_.clear();
        absolutePosition = fileSizeUsed > 0 ? fileSizeUsed : absolutePosition + recordSize;
        // The first user record of a split header file has no data in the index at creation.
        previousRecordSize = static_cast<uint32_t>(recordHeaderSize);
      }
      BREAK_ON_ERROR(file_.setPos(absolutePosition));
      continue;
    } else if (dataSize > 0) {
      if (absolutePosition + recordSize > totalFileSize_) {
        XR_LOGW(
            "Reindexing: record #{} truncated. {} bytes missing out of {} bytes.",
            index_.size(),
            absolutePosition + recordSize - totalFileSize_,
            recordSize);
        error = REINDEXING_ERROR;
        break;
      }
      if (file_.skipForward(dataSize) != 0) {
        error = file_.getLastError();
        XR_LOGW(
            "Reindexing: record #{}. Can't skip {} bytes of record data: {}",
            index_.size(),
            dataSize,
            errorCodeToMessage(error));
        break;
      }
    }
    if (recordableTypeId != RecordableTypeId::VRSIndex &&
        recordableTypeId != RecordableTypeId::VRSDescription) {
      // We read/skipped that record: it's "good", as far as we can tell. Add it to the index!
      StreamId streamId{recordHeader->getStreamId()};
      Record::Type recordType = recordHeader->getRecordType();
      if (isValid(recordType)) {
        streamIds_.insert(streamId);
        index_.emplace_back(recordHeader->timestamp.get(), absolutePosition, streamId, recordType);
        if (index_.size() > 1 && index_.back() < index_[index_.size() - 2]) {
          sortErrorCount_++;
        }
        if (diskIndex_) {
          diskIndex_->emplace_back(
              recordHeader->timestamp.get(), recordHeader->recordSize.get(), streamId, recordType);
        }
      } else {
        // We're probably in the weeds already
        XR_LOGW(
            "Reindexing: record #{}. Invalid record type: {}",
            index_.size(),
            static_cast<int>(recordHeader->recordType.get()));
        distrustLastRecord = true;
        error = REINDEXING_ERROR;
        break;
      }
    }
    absolutePosition += recordSize;
    previousRecordSize = recordSize;

    if (totalFileSize_ > 0) {
      if (!progressLogger_->logProgress("Reindexing", absolutePosition, totalFileSize_)) {
        return OPERATION_CANCELLED;
      }
    } else {
      if (!progressLogger_->logProgress("Reindexing")) {
        return OPERATION_CANCELLED;
      }
    }
  } while (true);
  if (error != 0 || distrustLastRecord) {
    // Printout the content of the broken header, for diagnostic purposes.
    XR_LOGI("Record #{} Header:", index_.size());
    XR_LOGI("Record Size: {}, expected {}", recordHeader->recordSize.get(), previousRecordSize);
    XR_LOGI("Previous Record Size: {}", recordHeader->previousRecordSize.get());
    XR_LOGI("Compression Type: {}", static_cast<int>(recordHeader->compressionType.get()));
    XR_LOGI("Uncompressed Size: {}", recordHeader->uncompressedSize.get());
    XR_LOGI("Timestamp: {}", recordHeader->timestamp.get());
    XR_LOGI("StreamId: {}", recordHeader->getStreamId().getName());
    XR_LOGI(
        "Record Type: {} ({})",
        toString(recordHeader->getRecordType()),
        static_cast<int>(recordHeader->recordType.get()));
    XR_LOGI("Format Version: {}", recordHeader->formatVersion.get());
  }
  if (distrustLastRecord && !index_.empty()) {
    XR_LOGW(
        "Reindexing: record #{}. Discarding last record, because it's suspicious.", index_.size());
    index_.pop_back(); // don't trust that last record
    absolutePosition -= previousRecordSize;
  }

  sort(index_.begin(), index_.end());
  XR_LOGI(
      "Indexing complete in {:.3f} sec. Found {} records and {} devices.",
      os::getTimestampSec() - beforeIndexing,
      index_.size(),
      streamIds_.size());
  if (!writeFixedIndex) {
    file_.forgetFurtherChunks(absolutePosition);
    return error;
  }
  XR_LOGW("Attempting to patch the index of a pre-existing VRS file.");
  Compressor compressor;
  do {
    BREAK_ON_ERROR(writeFile->reopenForUpdates());
    if (hasSplitHeadChunk_) {
      // re-write the index in the first chunk & update headers
      BREAK_ON_ERROR(writeFile->setPos(fileHeader_.indexRecordOffset.get()));
      BREAK_ON_ERROR(writeFile->read(recordHeader, recordHeaderSize));
      BREAK_ON_FALSE(recordHeader->getRecordableTypeId() == RecordableTypeId::VRSIndex);
      // Because mixing reads & writes requires a setpos, and that the read may have taken us
      // in the next chunk, we need to rewrite the record index, to overwrite the record
      BREAK_ON_ERROR(writeFile->setPos(fileHeader_.indexRecordOffset.get()));
      BREAK_ON_ERROR(writeFile->write(recordHeader, recordHeaderSize));
      uint32_t writtenIndexSize = 0;
      BREAK_ON_ERROR(writeDiskInfos(
          *writeFile, *diskIndex_, writtenIndexSize, compressor, kDefaultCompression));
      BREAK_ON_ERROR(writeFile->truncate());
      fileHeader_.firstUserRecordOffset.set(writeFile->getPos());
      BREAK_ON_ERROR(writeFile->setPos(0));
      BREAK_ON_ERROR(writeFile->write(fileHeader_));
      recordHeader->setCompressionType(CompressionType::Zstd);
      recordHeader->recordSize.set(static_cast<uint32_t>(recordHeaderSize + writtenIndexSize));
      recordHeader->uncompressedSize.set(
          static_cast<uint32_t>(sizeof(DiskRecordInfo) * index_.size()));
      BREAK_ON_ERROR(writeFile->setPos(fileHeader_.indexRecordOffset.get()));
      BREAK_ON_ERROR(writeFile->write(recordHeader, recordHeaderSize));
      XR_LOGI("Successfully updated the split index with {} records.", diskIndex_->size());
    } else {
      // At this point, "absolutePosition" is the first byte after the last complete record.
      // Write the index record at that location, and ignore everything past that.
      BREAK_ON_ERROR(writeFile->setPos(absolutePosition));
      uint32_t indexSize = previousRecordSize;
      BREAK_ON_ERROR(writeClassicIndexRecord(
          *writeFile, streamIds_, *diskIndex_, indexSize, compressor, kDefaultCompression));
      // maybe the chunk was larger (partial record). We can cut off possible extra bytes.
      BREAK_ON_ERROR(writeFile->truncate());
      // Go update the file's header to point to the index record.
      BREAK_ON_ERROR(writeFile->setPos(0));
      size_t minUpdateSize = offsetof(FileFormat::FileHeader, indexRecordOffset) +
          sizeof(fileHeader_.indexRecordOffset);
      fileHeader_.indexRecordOffset.set(absolutePosition);
      recordHeader->setCompressionType(CompressionType::None);
      recordHeader->uncompressedSize.set(0);
      BREAK_ON_ERROR(writeFile->overwrite(&fileHeader_, minUpdateSize));
      XR_LOGI("Successfully created an index for {} records.", diskIndex_->size());
    }
  } while (false); // fake loop to allow exit using break...
  if (error != 0) {
    XR_LOGE("File index reconstruction failed: the file is probably in a bad shape.");
  }
  return error;
}

int IndexRecord::Writer::preallocateClassicIndexRecord(
    WriteFileHandler& file,
    const deque<IndexRecord::DiskRecordInfo>& preliminaryIndex,
    uint32_t& outLastRecordSize) {
  int64_t indexRecordOffset = file.getPos();
  fileHeader_.enableFrontIndexRecordSupport(); // bump the file format version only if needed
  IF_ERROR_LOG_AND_RETURN(writeClassicIndexRecord(
      file,
      streamIds_,
      preliminaryIndex,
      outLastRecordSize,
      compressor_,
      kCompressionLevels[firstCompressionPresetIndex(preliminaryIndex.size())]));
  preallocatedIndexRecordSize_ = outLastRecordSize;
  // Re-write the file header immediately, in case writing is interrupted early
  fileHeader_.firstUserRecordOffset.set(file.getPos());
  IF_ERROR_LOG_AND_RETURN(file.setPos(0));
  IF_ERROR_LOG_AND_RETURN(file.overwrite(fileHeader_));
  IF_ERROR_LOG_AND_RETURN(file.setPos(fileHeader_.firstUserRecordOffset.get()));
  // Only save the index record's offset now, because we don't want to commit it to disk yet
  fileHeader_.indexRecordOffset.set(indexRecordOffset);
  return 0;
}

int IndexRecord::Writer::finalizeClassicIndexRecord(
    WriteFileHandler& file,
    int64_t endOfRecordsOffset,
    uint32_t& outLastRecordSize) {
  bool indexRecordWritten = false;
  int64_t descriptionRecordToIndexRecord =
      fileHeader_.indexRecordOffset.get() - fileHeader_.descriptionRecordOffset.get();
  // If space for the index record was pre-allocated, let's try to use it!
  if (preallocatedIndexRecordSize_ > 0 && descriptionRecordToIndexRecord > 0) {
    // We pre-allocated some space, using a preliminary index, which happens during copy operations.
    // Experimentally, using the same compression setting usually works, but sometimes, due to
    // approximations made during the creation of the preliminary index, it might fail.
    // In that case, we can try increasingly tighter compression levels to try to squeeze the data.
    // It's OK to possibly iterate, because copies are not real-time/capture operations.
    size_t retryIndex = firstCompressionPresetIndex(writtenRecords_.size());
    do {
      if (file.setPos(fileHeader_.indexRecordOffset.get()) == 0) {
        uint32_t lastRecordSize = static_cast<uint32_t>(descriptionRecordToIndexRecord);
        if (writeClassicIndexRecord(
                file,
                streamIds_,
                writtenRecords_,
                lastRecordSize,
                compressor_,
                kCompressionLevels[retryIndex],
                preallocatedIndexRecordSize_) == 0) {
          indexRecordWritten = true;
          outLastRecordSize = lastRecordSize;
        } else {
          if (kLogStats) {
            MAYBE_UNUSED size_t totalSize = sizeof(uint32_t) +
                streamIds_.size() * sizeof(DiskStreamId) + sizeof(uint32_t) +
                writtenRecords_.size() * sizeof(DiskRecordInfo);
            XR_LOGW(
                "Failed to use preallocated index. Wasted {} bytes reserved to compress {} bytes, "
                "{:.2f}% estimated.",
                preallocatedIndexRecordSize_,
                totalSize,
                preallocatedIndexRecordSize_ * 100.f / totalSize);
          }
        }
      }
    } while (!indexRecordWritten && ++retryIndex < kCompressionLevels.size());
  }
  // write the index at the end of the file if we need to
  int error = 0;
  if (!indexRecordWritten && (error = file.setPos(endOfRecordsOffset)) == 0) {
    fileHeader_.indexRecordOffset.set(endOfRecordsOffset);
    error = writeClassicIndexRecord(
        file, streamIds_, writtenRecords_, outLastRecordSize, compressor_, kDefaultCompression);
  }
  if (error == 0) {
    error = file.setPos(0);
  }
  if (error == 0) {
    error = file.overwrite(fileHeader_);
  }
  return error;
}

int Writer::createSplitIndexRecord(uint32_t& outLastRecordSize) {
  // Write the index record's record header (only)
  DiskFile& file = *splitHeadFile_;
  int64_t startOfIndex = file.getPos();
  splitIndexRecordHeader_.initIndexHeader(
      kSplitIndexFormatVersion, 0, outLastRecordSize, CompressionType::Zstd);
  WRITE_OR_LOG_AND_RETURN(file, &splitIndexRecordHeader_, sizeof(splitIndexRecordHeader_));
  outLastRecordSize = splitIndexRecordHeader_.recordSize.get();
  // Update & rewrite the file's header to tell where the index record is
  fileHeader_.indexRecordOffset.set(startOfIndex);
  IF_ERROR_LOG_AND_RETURN(file.setPos(0));
  IF_ERROR_LOG_AND_RETURN(file.overwrite(fileHeader_));
  // Move back after the index record's header
  IF_ERROR_LOG_AND_RETURN(
      file.setPos(startOfIndex + static_cast<int>(sizeof(splitIndexRecordHeader_))));
  return 0;
}

int Writer::addRecord(double timestamp, uint32_t size, StreamId id, Record::Type recordType) {
  writtenRecords_.emplace_back(timestamp, size, id, recordType);
  if (splitHeadFile_ && writtenRecords_.size() >= kMaxBatchSize) {
    return appendToSplitIndexRecord();
  }
  return 0;
}

int Writer::appendToSplitIndexRecord() {
  uint32_t writtenBytes = 0;
  int status = writeDiskInfos(
      *splitHeadFile_, writtenRecords_, writtenBytes, compressor_, kDefaultCompression);
  if (status == 0) {
    if (kLogStats) { // keeping around for now
      MAYBE_UNUSED float ratio =
          float(writtenBytes) / float(writtenRecords_.size() * sizeof(DiskRecordInfo));
      XR_LOGI(
          "comp: {} orig: {} ratio: {}",
          writtenBytes,
          writtenRecords_.size() * sizeof(DiskRecordInfo),
          ratio);
    }
    writtenBytesCount_ += writtenBytes;
    writtenIndexCount_ += writtenRecords_.size();
    writtenRecords_.clear();
  }
  return status;
}

int Writer::completeSplitIndexRecord() {
  DiskFile& file = *splitHeadFile_;
  int64_t offset = file.getPos();
  int error = writtenRecords_.empty() ? 0 : appendToSplitIndexRecord();
  if (error != 0) {
    XR_LOGW("Failed to write index details, error #{}, {}", error, errorCodeToMessage(error));
    if (offset > 0) {
      // let's try to remove what we wrote, as it's probably problematic!
      // If the failure happened because of a disk full error, we might be able to recover?
      if (file.setPos(offset) == 0 && file.truncate() == 0) {
        XR_LOGW(
            "It looks like we were able to truncate the file head, "
            "so the file should be recoverable");
      } else {
        XR_LOGE(
            "It looks like we were unable to truncate the file head, so the file is likely lost");
      }
    }
  } else {
    // now that we know the size of the index record, we can update the index record's record header
    // and the file's header to point to the first user record
    int64_t endOfIndexOffset = file.getPos();
    // rewrite the index record's record header
    splitIndexRecordHeader_.recordSize.set(
        static_cast<uint32_t>(sizeof(FileFormat::RecordHeader) + writtenBytesCount_));
    if (splitIndexRecordHeader_.getCompressionType() != CompressionType::None) {
      splitIndexRecordHeader_.uncompressedSize.set(
          static_cast<uint32_t>(writtenIndexCount_ * sizeof(DiskRecordInfo)));
    }
    IF_ERROR_LOG_AND_RETURN(file.setPos(fileHeader_.indexRecordOffset.get()));
    WRITE_OR_LOG_AND_RETURN(file, &splitIndexRecordHeader_, sizeof(splitIndexRecordHeader_));
    if (XR_VERIFY(endOfIndexOffset > 0)) {
      // update & rewrite the file's header
      fileHeader_.firstUserRecordOffset.set(endOfIndexOffset);
      IF_ERROR_LOG_AND_RETURN(file.setPos(0));
      IF_ERROR_LOG_AND_RETURN(file.overwrite(fileHeader_));
    } else {
      error = INDEX_RECORD_ERROR;
    }
  }
  return error;
}

int Writer::finalizeSplitIndexRecord(const unique_ptr<NewChunkHandler>& chunkHandler) {
  int finalizeStatus = completeSplitIndexRecord();
  NewChunkNotifier newChunkNotifier(*splitHeadFile_, chunkHandler);
  int closeStatus = splitHeadFile_->close();
  if (closeStatus != 0) {
    XR_LOGW(
        "Split head file closed with error #{}, {}", closeStatus, errorCodeToMessage(closeStatus));
  }
  newChunkNotifier.notify();
  return finalizeStatus != 0 ? finalizeStatus : closeStatus;
}

} // namespace vrs
