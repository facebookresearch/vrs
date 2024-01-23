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

#include "FileDetailsCache.h"

#define DEFAULT_LOG_CHANNEL "FileDetailsCache"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>

#include "Compressor.h"
#include "Decompressor.h"
#include "DescriptionRecord.h"
#include "DiskFile.h"
#include "ErrorCode.h"
#include "FileFormat.h"
#include "IndexRecord.h"
#include "Recordable.h"

namespace vrs {
namespace FileDetailsCache {

static const uint32_t kMagicHeader1 = FileFormat::fourCharCode('V', 'R', 'S', 'D');
static const uint32_t kMagicHeader2 = FileFormat::fourCharCode('e', 't', 'a', 'i');
static const uint32_t kMagicHeader3 = FileFormat::fourCharCode('l', 's', 'C', 'a');

static const uint32_t kOriginalFileFormatVersion = FileFormat::fourCharCode('V', 'R', 'S', 'a');

#pragma pack(push, 1)

/// \brief Helper class to store stream id on disk.
struct DiskStreamId {
  DiskStreamId() : typeId(static_cast<uint16_t>(RecordableTypeId::Undefined)), instanceId(0) {}
  explicit DiskStreamId(StreamId id)
      : typeId(static_cast<uint16_t>(id.getTypeId())), instanceId(id.getInstanceId()) {}

  FileFormat::LittleEndian<uint16_t> typeId;
  FileFormat::LittleEndian<uint16_t> instanceId;

  RecordableTypeId getTypeId() const {
    return static_cast<RecordableTypeId>(typeId.get());
  }

  uint16_t getInstanceId() const {
    return instanceId.get();
  }

  StreamId getStreamId() const {
    return {getTypeId(), getInstanceId()};
  }
};

/// \brief Helper class to store record information on disk.
struct DiskRecordInfo {
  DiskRecordInfo() = default;
  explicit DiskRecordInfo(const IndexRecord::RecordInfo& record)
      : timestamp(record.timestamp),
        recordOffset(record.fileOffset),
        streamId(record.streamId),
        recordType(static_cast<uint8_t>(record.recordType)) {}

  FileFormat::LittleEndian<double> timestamp;
  FileFormat::LittleEndian<int64_t> recordOffset;
  DiskStreamId streamId;
  FileFormat::LittleEndian<uint8_t> recordType;

  Record::Type getRecordType() const {
    return static_cast<Record::Type>(recordType.get());
  }

  StreamId getStreamId() const {
    return streamId.getStreamId();
  }
};

#pragma pack(pop)

/// File Format:
///
/// FileHeader: same struct as a regular VRS file, but with different magic numbers.
///   descriptionRecordOffset: offset of the description record (same as for a VRS file).
///   indexRecordOffset: offset of the index data, in its special case format.
///   firstUserRecordOffset: offset past the index data, effectively the end of the file.
///
/// Description record: same as for a regular VRS file.
///
/// Index data: custom for this use case, stream IDs and the index itself.
///   uint32_t recordableCount: count of StreamId structs, always present, value may be 0.
///   DiskStreamId streamId[recordableCount]: one per stream ID instance
///   uint32_t recordCount: count of DiskRecordInfo structs, always present, value may be 0.
///   DiskRecordInfo recordInfo[recordCount]; // one per actual record, zstd-frames compressed.
///
/// The file header's "future4" is used to save some flags, which may not have been set in the past:
///   - bit 0, when set, means the original VRS file is known to NOT have an index.

namespace {
const uint32_t kMaxBatchSize = 50000;
CompressionPreset kCompressionPreset = CompressionPreset::ZstdMedium;

enum FileHeaderFlag : uint64_t { FILE_HAS_NO_INDEX = 1 << 0 };

int writeRecordInfo(
    WriteFileHandler& file,
    const vector<IndexRecord::RecordInfo>& index,
    uint32_t& writtenSize) {
  // Write the count of records, and one RecordIndexStruct for each (in batch of fixed size)
  uint32_t recordsLeft = static_cast<uint32_t>(index.size());
  vector<DiskRecordInfo> recordStructs;
  writtenSize = 0;
  auto iter = index.begin();
  while (recordsLeft > 0) {
    uint32_t batchSize = (recordsLeft < kMaxBatchSize) ? recordsLeft : kMaxBatchSize;
    Compressor compressor;
    uint32_t frameSize = 0;
    IF_ERROR_RETURN(
        compressor.startFrame(batchSize * sizeof(DiskRecordInfo), kCompressionPreset, frameSize));
    recordStructs.resize(0);
    recordStructs.reserve(batchSize);
    for (uint32_t k = 0; k < batchSize; ++k, ++iter) {
      recordStructs.emplace_back(*iter);
    }
    size_t writeSize = sizeof(DiskRecordInfo) * batchSize;
    IF_ERROR_RETURN(compressor.addFrameData(file, recordStructs.data(), writeSize, frameSize));
    IF_ERROR_RETURN(compressor.endFrame(file, frameSize));
    recordsLeft -= batchSize;
    writtenSize += frameSize;
  }
  return 0;
}

int writeIndexData(
    WriteFileHandler& file,
    const set<StreamId>& streamIds,
    const vector<IndexRecord::RecordInfo>& index,
    size_t& outIndexSize) {
  // Write the count of streams, and one IndexRecord::DiskStreamId struct for each
  FileFormat::LittleEndian<uint32_t> recordableCount{static_cast<uint32_t>(streamIds.size())};
  WRITE_OR_LOG_AND_RETURN(file, &recordableCount, sizeof(recordableCount));
  vector<IndexRecord::DiskStreamId> diskStreams;
  diskStreams.reserve(streamIds.size());
  for (StreamId id : streamIds) {
    diskStreams.emplace_back(id);
  }
  size_t recordableWriteSize = sizeof(IndexRecord::DiskStreamId) * streamIds.size();
  WRITE_OR_LOG_AND_RETURN(file, diskStreams.data(), recordableWriteSize);

  FileFormat::LittleEndian<uint32_t> recordInfoCount{static_cast<uint32_t>(index.size())};
  WRITE_OR_LOG_AND_RETURN(file, &recordInfoCount, sizeof(recordInfoCount));

  uint32_t recordInfoSize = 0;
  IF_ERROR_LOG_AND_RETURN(writeRecordInfo(file, index, recordInfoSize));

  outIndexSize =
      sizeof(recordableCount) + recordableWriteSize + sizeof(recordInfoCount) + recordInfoSize;
  return 0;
}

int readIndexData(
    FileHandler& file,
    set<StreamId>& outStreamIds,
    vector<IndexRecord::RecordInfo>& outIndex,
    size_t indexSize) {
  FileFormat::LittleEndian<uint32_t> recordableCount;
  FileFormat::LittleEndian<uint32_t> diskIndexSize;
  if (!XR_VERIFY(indexSize >= sizeof(recordableCount))) {
    return FAILURE;
  }
  IF_ERROR_LOG_AND_RETURN(file.read(recordableCount));
  if (!XR_VERIFY(
          indexSize >= sizeof(recordableCount) +
              sizeof(IndexRecord::DiskStreamId) * recordableCount.get() + sizeof(diskIndexSize))) {
    return FAILURE;
  }
  vector<IndexRecord::DiskStreamId> diskStreams(recordableCount.get());
  IF_ERROR_LOG_AND_RETURN(
      file.read(diskStreams.data(), sizeof(IndexRecord::DiskStreamId) * recordableCount.get()));
  for (auto& id : diskStreams) {
    outStreamIds.insert(id.getStreamId());
  }
  IF_ERROR_LOG_AND_RETURN(file.read(diskIndexSize));
  outIndex.resize(0);
  outIndex.reserve(diskIndexSize.get());
  Decompressor decompressor;
  vector<DiskRecordInfo> diskRecords;
  size_t indexByteSize = indexSize - sizeof(recordableCount) -
      sizeof(IndexRecord::DiskStreamId) * recordableCount.get() - sizeof(diskIndexSize);
  while (outIndex.size() < diskIndexSize.get() && indexByteSize > 0) {
    size_t frameSize = 0;
    IF_ERROR_LOG_AND_RETURN(decompressor.initFrame(file, frameSize, indexByteSize));
    if (!XR_VERIFY(frameSize % sizeof(DiskRecordInfo) == 0)) {
      return FAILURE;
    }
    diskRecords.resize(frameSize / sizeof(DiskRecordInfo));
    IF_ERROR_LOG_AND_RETURN(
        decompressor.readFrame(file, diskRecords.data(), frameSize, indexByteSize));
    for (auto& diskRecord : diskRecords) {
      outIndex.emplace_back(
          diskRecord.timestamp.get(),
          diskRecord.recordOffset.get(),
          diskRecord.getStreamId(),
          static_cast<Record::Type>(diskRecord.recordType.get()));
    }
  }
  if (!XR_VERIFY(indexByteSize == 0 && outIndex.size() == diskIndexSize.get())) {
    return FAILURE;
  }
  return 0;
}

} // namespace

int write(
    const string& cacheFile,
    const set<StreamId>& streamIds,
    const map<string, string>& fileTags,
    const map<StreamId, StreamTags>& streamTags,
    const vector<IndexRecord::RecordInfo>& recordIndex,
    bool fileHasIndex) {
  AtomicDiskFile file;
  IF_ERROR_LOG_AND_RETURN(file.create(cacheFile));
  FileFormat::FileHeader fileHeader;
  fileHeader.init(kMagicHeader1, kMagicHeader2, kMagicHeader3, kOriginalFileFormatVersion);
  if (!fileHasIndex) {
    fileHeader.future4.set(FILE_HAS_NO_INDEX);
  }
  WRITE_OR_LOG_AND_RETURN(file, &fileHeader, sizeof(fileHeader));
  fileHeader.descriptionRecordOffset.set(file.getPos());
  map<StreamId, const StreamTags*> streamTagsMap;
  for (auto& rtags : streamTags) {
    streamTagsMap[rtags.first] = &rtags.second;
  }
  uint32_t descriptionSize = 0;
  IF_ERROR_LOG_AND_RETURN(
      DescriptionRecord::writeDescriptionRecord(file, streamTagsMap, fileTags, descriptionSize));
  fileHeader.indexRecordOffset.set(file.getPos());
  if (!XR_VERIFY(
          fileHeader.descriptionRecordOffset.get() + descriptionSize ==
          fileHeader.indexRecordOffset.get())) {
    return FAILURE;
  }
  size_t indexSize = 0;
  IF_ERROR_LOG_AND_RETURN(writeIndexData(file, streamIds, recordIndex, indexSize));
  fileHeader.firstUserRecordOffset.set(file.getPos());
  if (!XR_VERIFY(
          fileHeader.indexRecordOffset.get() + static_cast<int64_t>(indexSize) ==
          fileHeader.firstUserRecordOffset.get())) {
    return FAILURE;
  }
  IF_ERROR_LOG_AND_RETURN(file.setPos(0));
  WRITE_OR_LOG_AND_RETURN(file, &fileHeader, sizeof(fileHeader));
  return 0;
}

int read(
    const string& cacheFile,
    set<StreamId>& outStreamIds,
    map<string, string>& outFileTags,
    map<StreamId, StreamTags>& outStreamTags,
    vector<IndexRecord::RecordInfo>& outRecordIndex,
    bool& outFileHasIndex) {
  DiskFile file;
  IF_ERROR_LOG_AND_RETURN(file.open(cacheFile));
  int64_t fileSize = file.getTotalSize();
  FileFormat::FileHeader fileHeader;
  IF_ERROR_LOG_AND_RETURN(file.read(fileHeader));
  const int64_t descriptionOffset = fileHeader.descriptionRecordOffset.get();
  const int64_t indexRecordOffset = fileHeader.indexRecordOffset.get();
  const int64_t endOfFileOffset = fileHeader.firstUserRecordOffset.get();
  if (!XR_VERIFY(fileHeader.looksLikeOurFiles(kMagicHeader1, kMagicHeader2, kMagicHeader3)) ||
      !XR_VERIFY(fileHeader.fileFormatVersion.get() == kOriginalFileFormatVersion) ||
      !XR_VERIFY(descriptionOffset == sizeof(fileHeader) && descriptionOffset < fileSize) ||
      !XR_VERIFY(indexRecordOffset > descriptionOffset && indexRecordOffset < fileSize) ||
      !XR_VERIFY(endOfFileOffset > indexRecordOffset && endOfFileOffset == fileSize)) {
    return FAILURE;
  }
  IF_ERROR_LOG_AND_RETURN(file.setPos(descriptionOffset));
  uint32_t descriptionSize = 0;
  IF_ERROR_LOG_AND_RETURN(DescriptionRecord::readDescriptionRecord(
      file, fileHeader.recordHeaderSize.get(), descriptionSize, outStreamTags, outFileTags));
  if (!XR_VERIFY(descriptionOffset + descriptionSize == indexRecordOffset)) {
    return FAILURE;
  }
  size_t indexSize = static_cast<size_t>(endOfFileOffset - indexRecordOffset);
  IF_ERROR_LOG_AND_RETURN(readIndexData(file, outStreamIds, outRecordIndex, indexSize));
  outFileHasIndex = (fileHeader.future4.get() & FILE_HAS_NO_INDEX) == 0;
  return 0;
}

} // namespace FileDetailsCache
} // namespace vrs
