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

#include "Record.h"

#define DEFAULT_LOG_CHANNEL "VRSRecord"
#include <logging/Log.h>

#include <vrs/helpers/EnumStringConverter.h>
#include <vrs/helpers/FileMacros.h>

#include "Compressor.h"
#include "DataSource.h"
#include "FileFormat.h"
#include "RecordManager.h"

using namespace std;

namespace {
constexpr size_t kRecordHeaderSize = sizeof(vrs::FileFormat::RecordHeader);
}

namespace vrs {

namespace {
const char* sRecordTypes[] = {"Undefined", "State", "Configuration", "Data", "Tags"};
struct RecordTypeConverter : public EnumStringConverter<
                                 Record::Type,
                                 sRecordTypes,
                                 COUNT_OF(sRecordTypes),
                                 Record::Type::UNDEFINED,
                                 Record::Type::UNDEFINED> {
  static_assert(cNamesCount == enumCount<Record::Type>(), "Missing Record::Type name definitions");
};

} // namespace

string toString(Record::Type recordType) {
  return RecordTypeConverter::toString(recordType);
}

template <>
Record::Type toEnum<>(const string& name) {
  return RecordTypeConverter::toEnumNoCase(name.c_str());
}

const double Record::kMaxTimestamp = numeric_limits<double>::max();

void Record::recycle() {
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
  usedBufferSize_ = data.getDataSize();
  uint64_t allocateSize = kRecordHeaderSize + usedBufferSize_;
  // only resize if we have to
  if (buffer_.size() < allocateSize) {
    // If we're going to reallocate our buffer, then ask for a bit more right away...
    if (allocateSize > buffer_.capacity()) {
      buffer_.resize(0); // make sure we don't copy existing data for no reason!
    }
    buffer_.resize(allocateSize);
  }
  data.copyTo(&buffer_.data()->byte + kRecordHeaderSize);
  creationOrder_ = creationOrder;
}

bool Record::shouldTryToCompress() const {
  return Compressor::shouldTryToCompress(recordManager_.getCompression(), usedBufferSize_);
}

uint32_t Record::compressRecord(Compressor& compressor) {
  return compressor.compress(
      buffer_.data() + kRecordHeaderSize,
      usedBufferSize_,
      recordManager_.getCompression(),
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
    WRITE_OR_LOG_AND_RETURN(file, header, recordSize);
    inOutRecordSize = recordSize;
  }
  return 0;
}

const char* Record::typeName(Type type) {
  return RecordTypeConverter::toString(type);
}

} // namespace vrs
