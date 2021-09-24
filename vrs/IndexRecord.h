// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <deque>
#include <set>

#include "Compressor.h"
#include "DiskFile.h"
#include "FileFormat.h"
#include "ForwardDefinitions.h"
#include "NewChunkHandler.h"
#include "Record.h"

namespace vrs {

using std::deque;
using std::set;
using std::unique_ptr;
using std::vector;

class FileHandler;
class ProgressLogger;

struct DiskRecordIndexStruct;

namespace IndexRecord {

enum {
  kClassicIndexFormatVersion = 2, ///< Classic index record format, with a single index record/file
  /// Split index: Single index record, but potentially partial,
  /// with a dedicated file chunk for the VRS file's header, description & index records.
  kSplitIndexFormatVersion = 3,
};

#pragma pack(push, 1)

struct DiskStreamId {
  DiskStreamId() : typeId(static_cast<int32_t>(RecordableTypeId::Undefined)), instanceId(0) {}
  DiskStreamId(StreamId streamId)
      : typeId(static_cast<int32_t>(streamId.getTypeId())), instanceId(streamId.getInstanceId()) {}

  FileFormat::LittleEndian<int32_t> typeId;
  FileFormat::LittleEndian<uint16_t> instanceId;

  RecordableTypeId getTypeId() const {
    return FileFormat::readRecordableTypeId(typeId);
  }

  uint16_t getInstanceId() const {
    return instanceId.get();
  }

  StreamId getStreamId() const {
    return StreamId(getTypeId(), getInstanceId());
  }
};

struct DiskRecordInfo {
  DiskRecordInfo() {}
  DiskRecordInfo(double timestamp, uint32_t recordSize, StreamId streamId, Record::Type recordType)
      : timestamp(timestamp),
        recordSize(recordSize),
        recordType(static_cast<uint8_t>(recordType)),
        streamId(streamId) {}
  DiskRecordInfo(StreamId streamId, Record* record)
      : timestamp(record->getTimestamp()),
        recordSize(static_cast<uint32_t>(record->getSize())),
        recordType(static_cast<uint8_t>(record->getRecordType())),
        streamId(streamId) {}

  FileFormat::LittleEndian<double> timestamp;
  FileFormat::LittleEndian<uint32_t> recordSize;
  FileFormat::LittleEndian<uint8_t> recordType;
  DiskStreamId streamId;

  Record::Type getRecordType() const {
    return static_cast<Record::Type>(recordType.get());
  }

  StreamId getStreamId() const {
    return streamId.getStreamId();
  }
};

#pragma pack(pop)

struct RecordInfo {
  RecordInfo() {}
  RecordInfo(double timestamp, int64_t fileOffset, StreamId streamId, Record::Type recordType)
      : timestamp(timestamp), fileOffset(fileOffset), streamId(streamId), recordType(recordType) {}

  double timestamp; ///< timestamp of the record
  int64_t fileOffset; ///< absolute byte offset of the record in the whole file
  StreamId streamId; ///< creator of the record
  Record::Type recordType; ///< type of record

  bool operator<(const RecordInfo& rhs) const {
    return this->timestamp < rhs.timestamp ||
        (this->timestamp <= rhs.timestamp &&
         (this->streamId < rhs.streamId ||
          (this->streamId == rhs.streamId && this->fileOffset < rhs.fileOffset)));
  }

  bool operator==(const RecordInfo& rhs) const {
    return this->timestamp == rhs.timestamp && this->fileOffset == rhs.fileOffset &&
        this->streamId == rhs.streamId && this->recordType == rhs.recordType;
  }
};

class Writer {
 public:
  Writer(FileFormat::FileHeader& fileHeader)
      : fileHeader_{fileHeader}, preallocatedIndexRecordSize_{} {}

  void reset() {
    streamIds_.clear();
    writtenRecords_.clear();
    writtenBytesCount_ = 0;
    writtenIndexCount_ = 0;
    splitHeadFile_.reset();
  }

  DiskFile& initSplitHead() {
    splitHeadFile_ = std::make_unique<DiskFile>();
    return *splitHeadFile_;
  }

  bool hasSplitHead() const {
    return splitHeadFile_.get() != nullptr;
  }

  void addStream(StreamId id) {
    streamIds_.insert(id);
  }

  int addRecord(double timestamp, uint32_t size, StreamId id, Record::Type recordType);

  int preallocateClassicIndexRecord(
      WriteFileHandler& file,
      const deque<DiskRecordInfo>& preliminaryIndex,
      uint32_t& outLastRecordSize);
  void useClassicIndexRecord() {
    preallocatedIndexRecordSize_ = 0;
  }
  int finalizeClassicIndexRecord(
      WriteFileHandler& file,
      int64_t endOfRecordsOffset,
      uint32_t& outLastRecordSize);

  int createSplitIndexRecord(uint32_t& outLastRecordSize);
  int finalizeSplitIndexRecord(const unique_ptr<NewChunkHandler>& chunkHandler);

 protected:
  int appendToSplitIndexRecord();
  int completeSplitIndexRecord();

 private:
  std::unique_ptr<DiskFile> splitHeadFile_; // When the file head is split from the user records
  FileFormat::FileHeader& fileHeader_;
  FileFormat::RecordHeader splitIndexRecordHeader_;
  uint32_t preallocatedIndexRecordSize_;
  Compressor compressor_;
  set<StreamId> streamIds_;
  deque<IndexRecord::DiskRecordInfo> writtenRecords_;
  size_t writtenBytesCount_; // how many bytes have been written in a partial index
  size_t writtenIndexCount_; // how many index entries have been written in the partial index
};

class Reader {
 public:
  Reader(
      FileHandler& file,
      FileFormat::FileHeader& fileHeader,
      ProgressLogger* progressLogger,
      set<StreamId>& outStreamIds,
      vector<RecordInfo>& outIndex);

  bool isIndexComplete() const {
    return indexComplete_;
  }

  int readRecord(int64_t firstUserRecordOffset, int64_t& outUsedFileSize);

  /// Rebuild the index of an open file.
  /// @param writeFixedIndex: true to path the file with the rebuilt index.
  /// @param fileHeader: the file's index.
  /// @return 0 on success, or a non-zero error code.
  int rebuildIndex(bool writeFixedIndex);

 private:
  int readRecord(
      int64_t indexRecordOffset,
      int64_t firstUserRecordOffset,
      int64_t& outUsedFileSize);
  int readClassicIndexRecord(
      size_t indexRecordPayloadSize,
      size_t uncompressedSize,
      int64_t firstUserRecordOffset,
      int64_t& outUsedFileSize);
  int readSplitIndexRecord(size_t indexByteSize, size_t uncompressedSize, int64_t& outUsedFileSize);
  int readDiskInfo(vector<DiskRecordInfo>& outRecords);

 private:
  FileHandler& file_;
  const int64_t totalFileSize_;
  FileFormat::FileHeader& fileHeader_;
  ProgressLogger* progressLogger_;
  set<StreamId>& streamIds_;
  vector<RecordInfo>& index_;
  unique_ptr<deque<IndexRecord::DiskRecordInfo>> diskIndex_; // only when rewritting the index
  bool indexComplete_;
  bool hasSplitHeadChunk_;
  int32_t sortErrorCount_;
  int32_t droppedRecordCount_;
};

/// This is used to count records to different kinds
struct RecordSignature {
  StreamId streamId;
  Record::Type recordType;

  bool operator<(const RecordSignature& rhs) const {
    return this->recordType < rhs.recordType ||
        (this->recordType == rhs.recordType && this->streamId < rhs.streamId);
  }
};

} // namespace IndexRecord

} // namespace vrs
