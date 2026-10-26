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

#include <vrs/Record.h>

#include <vector>

#define DEFAULT_LOG_CHANNEL "VRSRecord"
#include <logging/Log.h>

#include <vrs/Compressor.h>
#include <vrs/DataSource.h>
#include <vrs/FileFormat.h>
#include <vrs/RecordManager.h>
#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/FileMacros.h>

using namespace std;

namespace {
constexpr size_t kRecordHeaderSize = sizeof(vrs::FileFormat::RecordHeader);

string_view sRecordTypes[] = {"Undefined", "State", "Configuration", "Data", "Tags"};

using RecordType = vrs::Record::Type;
ENUM_STRING_CONVERTER(RecordType, sRecordTypes, RecordType::UNDEFINED);

} // namespace

namespace vrs {

string toString(Record::Type recordType) {
  return RecordTypeConverter::toString(recordType);
}

template <>
Record::Type toEnum<>(const string& name) {
  return RecordTypeConverter::toEnumNoCase(name.c_str());
}

const double Record::kMaxTimestamp = numeric_limits<double>::max();

Record::Record(RecordManager& recordManager) : recordManager_(recordManager) {}

Record::~Record() = default;

void Record::recycle() {
  directWriteRecordData_.reset();
  borrowedData_.reset();
  recordManager_.recycle(this);
}

void Record::set(
    double timestamp,
    Type type,
    uint32_t formatVersion,
    const DataSource& data,
    uint64_t creationOrder) {
  timestamp_ = timestamp;
  recordType_ = type;
  formatVersion_ = formatVersion;
  ownedSize_ = data.getDataSize();
  usedBufferSize_ = ownedSize_;
  uint64_t allocateSize = kRecordHeaderSize + usedBufferSize_;
  // writeRecord() either initializes this header or uses a separate compressed buffer.
  buffer_.resizeDiscardingWithoutInitialization(allocateSize);
  data.copyTo(buffer_.data() + kRecordHeaderSize);
  creationOrder_ = creationOrder;
  directWriteRecordData_.reset();
  borrowedData_.reset();
}

void Record::addDirectWriteRecordData(std::unique_ptr<DirectWriteRecordData>&& directWriteData) {
  directWriteRecordData_ = std::move(directWriteData);
  usedBufferSize_ += directWriteRecordData_->getDataSize();
}

void Record::addBorrowedData(std::unique_ptr<BorrowedData>&& borrowedData) {
  if (borrowedData_) {
    usedBufferSize_ -= borrowedData_->getDataSize();
  }
  borrowedData_ = std::move(borrowedData);
  if (borrowedData_) {
    usedBufferSize_ += borrowedData_->getDataSize();
  }
}

bool Record::shouldTryToCompress() const {
  return !directWriteRecordData_ &&
      Compressor::shouldTryToCompress(recordManager_.getCompression(), usedBufferSize_);
}

int Record::compressRecord(Compressor& compressor, uint32_t& outCompressedSize) {
  compressorDataSlices_.clear();
  compressorDataSlices_.reserve(1 + (borrowedData_ ? borrowedData_->getSegments().size() : 0));
  compressorDataSlices_.push_back(DataSlice{buffer_.data() + kRecordHeaderSize, ownedSize_});
  if (borrowedData_) {
    const auto& borrowedSegments = borrowedData_->getSegments();
    compressorDataSlices_.insert(
        compressorDataSlices_.end(), borrowedSegments.begin(), borrowedSegments.end());
  }
  return compressor.compress(
      compressorDataSlices_.data(),
      compressorDataSlices_.size(),
      recordManager_.getCompression(),
      outCompressedSize,
      kRecordHeaderSize);
}

int Record::writeRecord(
    WriteFileHandler& file,
    StreamId streamId,
    uint32_t& inOutRecordSize,
    Compressor& compressor,
    uint32_t compressedSize) {
  CompressionType compressionType = compressor.getCompressionType();
  if (compressionType != CompressionType::None && compressedSize > 0) {
    uint32_t recordSize = static_cast<uint32_t>(kRecordHeaderSize + compressedSize);
    auto* header = compressor.getHeader<FileFormat::RecordHeader>();
    header->initHeader(
        getRecordType(),
        streamId,
        timestamp_,
        formatVersion_,
        compressionType,
        inOutRecordSize,
        recordSize,
        static_cast<uint32_t>(usedBufferSize_));
    WRITE_OR_LOG_AND_RETURN(file, header, recordSize);
    inOutRecordSize = recordSize;
    // The borrowed bytes are already in the compressor's buffer; release the owner now.
    borrowedData_.reset();
  } else {
    uint32_t recordSize = static_cast<uint32_t>(kRecordHeaderSize + usedBufferSize_);
    auto* header = reinterpret_cast<FileFormat::RecordHeader*>(buffer_.data());
    header->initHeader(
        getRecordType(),
        streamId,
        timestamp_,
        formatVersion_,
        CompressionType::None,
        inOutRecordSize,
        recordSize,
        0);
    // Borrowed and direct-write data aren't in buffer_; append them raw after the copied payload.
    const size_t tailSize = (borrowedData_ ? borrowedData_->getDataSize() : 0) +
        (directWriteRecordData_ ? directWriteRecordData_->getDataSize() : 0);
    WRITE_OR_LOG_AND_RETURN(file, header, recordSize - tailSize);
    if (borrowedData_) {
      for (const DataSlice& segment : borrowedData_->getSegments()) {
        WRITE_OR_LOG_AND_RETURN(file, segment.data, segment.size);
      }
      borrowedData_.reset();
    }
    if (directWriteRecordData_) {
      IF_ERROR_RETURN(directWriteRecordData_->write(file));
      directWriteRecordData_.reset();
    }
    inOutRecordSize = recordSize;
  }
  return 0;
}

const char* Record::typeName(Record::Type type) {
  return RecordTypeConverter::toCString(type);
}

} // namespace vrs
