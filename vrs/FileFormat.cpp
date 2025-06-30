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

#include "FileFormat.h"

#include <ctime>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

#define DEFAULT_LOG_CHANNEL "FileFormat"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Strings.h>

#include "ErrorCode.h"
#include "IndexRecord.h"
#include "ProgressLogger.h"

using namespace std;
using namespace std::chrono;

namespace vrs {

namespace FileFormat {

constexpr uint32_t kMagicHeader1 = fourCharCode('V', 'i', 's', 'i');
constexpr uint32_t kMagicHeader2 = fourCharCode('o', 'n', 'R', 'e');
constexpr uint32_t kMagicHeader3 = fourCharCode('c', 'o', 'r', 'd');

/// Original file format
constexpr uint32_t kOriginalFileFormatVersion = fourCharCode('V', 'R', 'S', '1');
/// When we added support for place the index record at the beginning of the file
constexpr uint32_t kFrontIndexFileFormatVersion = fourCharCode('V', 'R', 'S', '2');
/// When we added support for ztd compression. Used only briefly.
constexpr uint32_t kZstdFormatVersion = fourCharCode('V', 'R', 'S', '3');

void FileHeader::init() {
  init(kMagicHeader1, kMagicHeader2, kMagicHeader3, kOriginalFileFormatVersion);
}

void FileHeader::init(uint32_t magic1, uint32_t magic2, uint32_t magic3, uint32_t formatVersion) {
  magicHeader1.set(magic1);
  magicHeader2.set(magic2);
  magicHeader3.set(magic3);
  fileHeaderSize.set(sizeof(FileHeader));
  recordHeaderSize.set(sizeof(RecordHeader));
  uint64_t id = static_cast<uint64_t>(
      duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
  // The 64 bit creationId might be used to identify the file to cache its index: it must be unique.
  // Most nanosecond implementations don't return values with nanosecond precision,
  // so we override the 30 lsb (~1s) with random bits.
  // creationId is now an approximate number of ns since Unix EPOCH, with 30 bits guaranteed random.
  // Note: this is not perfect uniqueness, but good enough to avoid collisions in a local file
  // cache.
  random_device rd;
  mt19937_64 engine(rd());
  uniform_int_distribution<uint32_t> distribution;
  uint32_t randomBits = distribution(engine);
  const uint64_t c30bits = (1 << 30) - 1; // 30 lsb set
  id = (id & ~c30bits) | (randomBits & c30bits);
  creationId.set(id);
  fileFormatVersion.set(formatVersion);
}

bool FileHeader::looksLikeAVRSFile() const {
  return looksLikeOurFiles(kMagicHeader1, kMagicHeader2, kMagicHeader3);
}

bool FileHeader::looksLikeOurFiles(uint32_t magic1, uint32_t magic2, uint32_t magic3) const {
  // Check magic values
  if (magicHeader1.get() != magic1 || magicHeader2.get() != magic2 ||
      magicHeader3.get() != magic3) {
    return false;
  }
  // file & record headers are required to only grow
  if (fileHeaderSize.get() < sizeof(FileHeader) || recordHeaderSize.get() < sizeof(RecordHeader)) {
    return false;
  }
  // It's extremely unlikely that the file & record headers will grow "a lot": filter based on that
  const size_t maxHeaderGrowth = 200;
  return fileHeaderSize.get() <= sizeof(FileHeader) + maxHeaderGrowth &&
      recordHeaderSize.get() <= sizeof(RecordHeader) + maxHeaderGrowth;
}

bool FileHeader::isFormatSupported() const {
  uint32_t version = fileFormatVersion.get();
  return version == kOriginalFileFormatVersion || version == kFrontIndexFileFormatVersion ||
      version == kZstdFormatVersion;
}

void FileHeader::enableFrontIndexRecordSupport() {
  fileFormatVersion.set(kFrontIndexFileFormatVersion);
}

int64_t FileHeader::getEndOfUserRecordsOffset(int64_t fileSize) const {
  if (looksLikeAVRSFile()) {
    // index maybe before or after the user records
    if (indexRecordOffset.get() > firstUserRecordOffset.get()) {
      return min<int64_t>(fileSize, indexRecordOffset.get());
    }
  }
  return fileSize;
}

RecordHeader::RecordHeader() = default;

RecordHeader::RecordHeader(
    Record::Type recordType,
    StreamId streamId,
    double timestamp,
    uint32_t formatVersion,
    CompressionType compressionType,
    uint32_t previousRecordSize,
    uint32_t recordSize,
    uint32_t uncompressedSize) {
  this->recordSize.set(recordSize);
  this->previousRecordSize.set(previousRecordSize);
  this->recordableTypeId.set(static_cast<int32_t>(streamId.getTypeId()));
  this->formatVersion.set(formatVersion);
  this->timestamp.set(timestamp);
  this->recordableInstanceId.set(streamId.getInstanceId());
  setRecordType(recordType);
  setCompressionType(compressionType);
  this->uncompressedSize.set(uncompressedSize);
}

void RecordHeader::initHeader(
    Record::Type _recordType,
    StreamId _streamId,
    double _timestamp,
    uint32_t _formatVersion,
    CompressionType _compressionType,
    uint32_t _previousRecordSize,
    uint32_t _recordSize,
    uint32_t _uncompressedSize) {
  this->recordSize.set(_recordSize);
  this->previousRecordSize.set(_previousRecordSize);
  this->recordableTypeId.set(static_cast<int32_t>(_streamId.getTypeId()));
  this->formatVersion.set(_formatVersion);
  this->timestamp.set(_timestamp);
  this->recordableInstanceId.set(_streamId.getInstanceId());
  setRecordType(_recordType);
  setCompressionType(_compressionType);
  this->uncompressedSize.set(_uncompressedSize);
}

void RecordHeader::initIndexHeader(
    uint32_t _formatVersion,
    uint32_t indexSize,
    uint32_t _previousRecordSize,
    CompressionType _compressionType) {
  setRecordType(Record::Type::DATA);
  recordSize.set(sizeof(RecordHeader) + indexSize);
  this->previousRecordSize.set(_previousRecordSize);
  this->formatVersion.set(_formatVersion);
  this->recordableTypeId.set(static_cast<int32_t>(RecordableTypeId::VRSIndex));
  this->timestamp.set(Record::kMaxTimestamp);
  setCompressionType(_compressionType);
}

void RecordHeader::initDescriptionHeader(
    uint32_t _formatVersion,
    uint32_t descriptionRecordSize,
    uint32_t _previousRecordSize) {
  setRecordType(Record::Type::DATA);
  recordSize.set(descriptionRecordSize);
  this->previousRecordSize.set(_previousRecordSize);
  this->formatVersion.set(_formatVersion);
  this->recordableTypeId.set(static_cast<int32_t>(RecordableTypeId::VRSDescription));
  this->timestamp.set(Record::kMaxTimestamp);
}

bool RecordHeader::isSanityCheckOk() const {
  if (!XR_VERIFY(recordSize.get() >= sizeof(RecordHeader)) ||
      !XR_VERIFY(
          previousRecordSize.get() == 0 || previousRecordSize.get() >= sizeof(RecordHeader))) {
    return false;
  }
  if (!XR_VERIFY(recordType.get() > static_cast<uint8_t>(Record::Type::UNDEFINED)) ||
      !XR_VERIFY(recordType.get() < static_cast<uint8_t>(Record::Type::COUNT))) {
    return false;
  }
  uint32_t uncompressedPayload = uncompressedSize.get(); // doesn't include header already
  if (uncompressedPayload > 0) {
    if (getRecordableTypeId() != RecordableTypeId::VRSIndex) {
      uint32_t compressedPayload = recordSize.get() - sizeof(RecordHeader);
      // we did not always check that compression actually helped, and smaller sizes do worse
      uint32_t maxIncrease = (uncompressedPayload < 200)
          ? max<size_t>(50, uncompressedPayload / 2) // 50 bytes or 50%
          : max<size_t>(100, uncompressedPayload * 5ULL / 100); // 100 bytes or 5%
      if (!XR_VERIFY(compressedPayload < uncompressedPayload + maxIncrease)) {
        return false;
      }
    }
    if (!XR_VERIFY(compressionType.get() != static_cast<uint8_t>(CompressionType::None)) ||
        !XR_VERIFY(compressionType.get() < static_cast<uint8_t>(CompressionType::COUNT))) {
      return false;
    }
  }
  return true;
}

RecordableTypeId readRecordableTypeId(const FileFormat::LittleEndian<int32_t>& recordableTypeId) {
  int32_t rawTypeId = recordableTypeId.get();
  // reinterpret ids for test & sample devices in their legacy space...
  int32_t kLegacyTestDevices = 100000;
  if (rawTypeId >= kLegacyTestDevices) {
    rawTypeId =
        (rawTypeId - kLegacyTestDevices) + static_cast<int32_t>(RecordableTypeId::TestDevices);
  }
  return static_cast<RecordableTypeId>(rawTypeId);
}

bool printVRSFileInternals(unique_ptr<FileHandler>& file) {
  using namespace std;
  TemporaryCachingStrategy temporaryCachingStrategy(file, CachingStrategy::Passive);
  cout << "FileHandler: " << file->getFileHandlerName() << "\n";
  FileFormat::FileHeader fileHeader;
  int error = file->read(fileHeader);
  if (error != 0) {
    cerr << "Can't read file header, error #" << error << ": " << errorCodeToMessage(error) << "\n";
    return false;
  }
  // Let's check the file header...
  if (fileHeader.looksLikeAVRSFile()) {
    cout << "File header integrity: OK.\n";
  } else {
    cerr << "File header integrity check failed. This is not a VRS file.\n";
    return false;
  }
  bool returnValue = true;
  uint32_t fileFormatVersion = fileHeader.fileFormatVersion.get();
  cout << "File format version: '" << char(fileFormatVersion & 0xff)
       << char((fileFormatVersion >> 8) & 0xff) << char((fileFormatVersion >> 16) & 0xff)
       << char((fileFormatVersion >> 24) & 0xff) << "', "
       << (fileHeader.isFormatSupported() ? "supported." : "NOT SUPPORTED.") << "\n";
  cout << "Creation ID: " << hex << fileHeader.creationId.get() << dec << ".\n";
  time_t creationTimeSec = static_cast<time_t>(fileHeader.creationId.get() / 1000000000);
  cout << "Creation date: " << put_time(localtime(&creationTimeSec), "%c %Z.") << '\n';
  cout << "File header size: " << fileHeader.fileHeaderSize.get() << " bytes";
  if (fileHeader.fileHeaderSize.get() == sizeof(fileHeader)) {
    cout << ", as expected.\n";
  } else {
    cout << ", compared to " << sizeof(fileHeader) << " bytes expected.\n";
  }
  cout << "Record header size: " << fileHeader.recordHeaderSize.get() << " bytes";
  if (fileHeader.recordHeaderSize.get() == sizeof(FileFormat::RecordHeader)) {
    cout << ", as expected.\n";
  } else {
    cout << ", compared to " << sizeof(FileFormat::RecordHeader) << " bytes expected.\n";
  }
  bool descriptionRecordAfterFileHeader =
      fileHeader.descriptionRecordOffset.get() == fileHeader.fileHeaderSize.get();
  cout << "Description record offset: " << fileHeader.descriptionRecordOffset.get() << ", "
       << (descriptionRecordAfterFileHeader ? "right after the file header, as expected."
                                            : "NOT RIGHT AFTER THE FILE HEADER")
       << "\n";
  if (!descriptionRecordAfterFileHeader) {
    returnValue = false;
  }

  // Check description record header
  FileFormat::RecordHeader descriptionRecordHeader;
  IF_ERROR_LOG(file->setPos(fileHeader.descriptionRecordOffset.get()));
  IF_ERROR_LOG(file->read(descriptionRecordHeader));

  cout << "Description record size: "
       << helpers::humanReadableFileSize(descriptionRecordHeader.recordSize.get()) << ".\n";
  int64_t indexRecordOffset = fileHeader.indexRecordOffset.get();
  cout << "Index record offset: " << indexRecordOffset << ", ";
  if (indexRecordOffset ==
      fileHeader.fileHeaderSize.get() + descriptionRecordHeader.recordSize.get()) {
    cout << "right after the description record (Ready for streaming).\n";
  } else {
    if (indexRecordOffset == 0) {
      indexRecordOffset =
          fileHeader.fileHeaderSize.get() + descriptionRecordHeader.recordSize.get();
      cout << "anticipated at " << indexRecordOffset << ", after the description record.\n";
    } else {
      cout << "NOT after the description record. Not great for streaming.\n";
    }
  }

  // Check index record header
  FileFormat::RecordHeader indexRecordHeader;
  IF_ERROR_LOG(file->setPos(indexRecordOffset));
  IF_ERROR_LOG(file->read(indexRecordHeader));

  cout << "Index Record size: "
       << helpers::humanReadableFileSize(indexRecordHeader.recordSize.get()) << ".\n";
  if (indexRecordHeader.recordSize.get() == fileHeader.recordHeaderSize.get()) {
    cout << "This index record looks empty\n";
  } else if (indexRecordHeader.recordSize.get() < fileHeader.recordHeaderSize.get()) {
    cerr << "This is smaller than a record header, so something's really off!\n";
    returnValue = false;
  } else if (
      indexRecordHeader.getCompressionType() != CompressionType::None ||
      indexRecordHeader.uncompressedSize.get() != 0) {
    cout << "Index Record uncompressed size: "
         << helpers::humanReadableFileSize(indexRecordHeader.uncompressedSize.get())
         << ", compressed with " << toString(indexRecordHeader.getCompressionType()) << ".\n";
  }
  int64_t offsetBefore = file->getPos();
  set<StreamId> streamIds;
  vector<IndexRecord::RecordInfo> records;
  ProgressLogger logger;
  IndexRecord::Reader indexReader(*file, fileHeader, &logger, streamIds, records);
  int64_t usedFileSize{};
  int status = indexReader.readRecord(fileHeader.firstUserRecordOffset.get(), usedFileSize);
  if (status != 0) {
    cerr << "Can't read index record, error " << errorCodeToMessageWithCode(status) << "\n";
    returnValue = false;
  } else {
    int64_t indexReadSize = file->getPos() - offsetBefore;
    cout << "Index Record contains " << records.size() << " records, "
         << helpers::humanReadableFileSize(records.size() * sizeof(IndexRecord::DiskRecordInfo))
         << " worth of data, " << helpers::humanReadableFileSize(indexReadSize) << " compressed";
    if (indexRecordHeader.uncompressedSize.get() > 0) {
      double ratio = ((indexRecordHeader.uncompressedSize.get() - indexReadSize) * 100.0) /
          indexRecordHeader.uncompressedSize.get();
      cout << fmt::format(", {:.2f}% saved", ratio);
    }
    if (indexRecordHeader.recordSize.get() > indexReadSize) {
      double usage = (indexReadSize * 100.0) / indexRecordHeader.recordSize.get();
      cout << fmt::format(", record {:.2f}% used", usage);
    }
    cout << ".\n";
  }
  int64_t endOfSplitIndexRecordOffset = 0;
  uint32_t indexFormatVersion = indexRecordHeader.formatVersion.get();
  cout << "Index Record format version: ";
  if (indexFormatVersion == vrs::IndexRecord::kClassicIndexFormatVersion) {
    cout << "Classic.\n";
  } else if (indexFormatVersion == vrs::IndexRecord::kSplitIndexFormatVersion) {
    cout << "Split File Head.\n";
    int64_t currentPos = file->getPos();
    int64_t chunkStart{}, chunkSize{};
    if (file->getChunkRange(chunkStart, chunkSize) == 0 &&
        XR_VERIFY(currentPos >= chunkStart && currentPos < chunkStart + chunkSize)) {
      int64_t nextChunkStart = chunkStart + chunkSize;
      if (chunkStart == 0) {
        int64_t indexByteSize = nextChunkStart - currentPos;
        cout << "Split index size (bytes left in first chunk): " << indexByteSize << " bytes, or ";
        size_t leftover =
            static_cast<size_t>(indexByteSize) % sizeof(vrs::IndexRecord::DiskRecordInfo);
        size_t count =
            static_cast<size_t>(indexByteSize) / sizeof(vrs::IndexRecord::DiskRecordInfo);
        if (leftover == 0) {
          cout << "precisely " << count << " records.\n";
        } else {
          cout << count << " records, and " << leftover << " extra bytes (not good!)\n";
          returnValue = false;
        }
        endOfSplitIndexRecordOffset = nextChunkStart;
      } else {
        // We're already in the next chunk: is the index empty?
        if (chunkStart == currentPos) {
          cout << "Split index empty.\n";
          endOfSplitIndexRecordOffset = chunkStart;
        } else {
          // something's really off...
          cerr << "Split index error! Ends at " << currentPos << ", but the first chunk is from "
               << chunkStart << " to " << nextChunkStart - 1 << ".\n";
          returnValue = false;
        }
      }
    } else {
      cerr << "Can't get current chunk information!\n";
      returnValue = false;
    }
  } else {
    cerr << "Unknown! (" << indexFormatVersion << ").\n";
    returnValue = false;
  }

  int64_t firstUserRecordOffset = fileHeader.firstUserRecordOffset.get();
  cout << "First user record offset: " << firstUserRecordOffset << ", ";
  if (firstUserRecordOffset == 0) {
    cout << "value not set";
    if (indexFormatVersion == vrs::IndexRecord::kClassicIndexFormatVersion) {
      cout << ", which is expected with legacy files, pre-streaming optimizations.\n";
      int64_t endOfDescriptionRecord =
          fileHeader.descriptionRecordOffset.get() + descriptionRecordHeader.recordSize.get();
      if (endOfDescriptionRecord < fileHeader.indexRecordOffset.get()) {
        cout << "First user record at " << endOfDescriptionRecord
             << ", after the description record.\n";
        firstUserRecordOffset = endOfDescriptionRecord;
      }
    } else if (indexFormatVersion == vrs::IndexRecord::kSplitIndexFormatVersion) {
      cout << ", which means the recording was probably interrupted.\n";
    } else {
      cout << ".\n";
    }
  } else {
    cout << "value set, when doing streaming optimizations.\n";
  }
  if (endOfSplitIndexRecordOffset != 0) {
    cout << "End of split index record: " << endOfSplitIndexRecordOffset << ".\n";
  }

  if (firstUserRecordOffset != 0 && endOfSplitIndexRecordOffset != 0) {
    if (firstUserRecordOffset != endOfSplitIndexRecordOffset) {
      cout << "The end of the index record doesn't match the location of the first user record!\n";
    }
  } else if (firstUserRecordOffset == 0 && endOfSplitIndexRecordOffset != 0) {
    firstUserRecordOffset = endOfSplitIndexRecordOffset;
  }

  if (firstUserRecordOffset == 0) {
    cerr << "We don't know where the first user record is.\n";
    returnValue = false;
  } else {
    FileFormat::RecordHeader firstUserRecord;
    IF_ERROR_LOG(file->setPos(firstUserRecordOffset));
    IF_ERROR_LOG(file->read(firstUserRecord));
    cout << "Size of record before first user record: " << firstUserRecord.previousRecordSize.get()
         << " bytes.\n";
  }

  return returnValue;
}

} // namespace FileFormat

} // namespace vrs
