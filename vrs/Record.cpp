// Facebook Technologies, LLC Proprietary and Confidential.

#include "Record.h"

#define DEFAULT_LOG_CHANNEL "VRSRecord"
#include <logging/Log.h>

#include <vrs/utils/EnumStringConverter.h>
#include "Compressor.h"
#include "DataSource.h"
#include "ErrorCode.h"
#include "FileFormat.h"
#include "FileMacros.h"
#include "RecordManager.h"

namespace vrs {

namespace {
const char* sRecordTypes[] = {"Undefined", "State", "Configuration", "Data", "Tags"};
struct RecordTypeConverter : public EnumStringConverter<
                                 Record::Type,
                                 sRecordTypes,
                                 COUNT_OF(sRecordTypes),
                                 Record::Type::UNDEFINED,
                                 Record::Type::UNDEFINED> {
  static_assert(
      cNamesCount == static_cast<size_t>(Record::Type::COUNT),
      "Missing Record::Type name definitions");
};

} // namespace

std::string toString(Record::Type recordType) {
  return RecordTypeConverter::toString(recordType);
}

template <>
Record::Type toEnum<>(const std::string& name) {
  return RecordTypeConverter::toEnumNoCase(name.c_str());
}

const double Record::kMaxTimestamp = DBL_MAX;

void Record::recycle() {
  recordManager_.recycle(this);
}

void Record::set(
    double timestamp,
    Type type,
    uint32_t formatVersion,
    const DataSource& data,
    uint64_t creationOrder) {
  this->timestamp_ = timestamp;
  this->recordType_ = type;
  this->formatVersion_ = formatVersion;
  bufferUsedSize_ = data.getDataSize();
  // only resize if we have to
  if (buffer_.size() < bufferUsedSize_) {
    // If we're going to reallocate our buffer, then ask for a bit more right away...
    if (bufferUsedSize_ > buffer_.capacity()) {
      buffer_.reserve(recordManager_.getAdjustedRecordBufferSize(bufferUsedSize_));
    }
    buffer_.resize(bufferUsedSize_);
  }
  data.copyTo(buffer_.data());
  this->creationOrder_ = creationOrder;
}

bool Record::shouldTryToCompress() const {
  return Compressor::shouldTryToCompress(recordManager_.getCompression(), bufferUsedSize_);
}

uint32_t Record::compressRecord(Compressor& compressor) {
  return compressor.compress(buffer_.data(), bufferUsedSize_, recordManager_.getCompression());
}

int Record::writeRecord(
    WriteFileHandler& file,
    StreamId streamId,
    uint32_t& inOutRecordSize,
    Compressor& compressor,
    uint32_t compressedSize) {
  CompressionType compressionType = compressor.getCompressionType();
  if (compressionType != CompressionType::None && compressedSize > 0) {
    uint32_t recordSize = static_cast<uint32_t>(sizeof(FileFormat::RecordHeader) + compressedSize);
    FileFormat::RecordHeader recordHeader(
        getRecordType(),
        streamId,
        timestamp_,
        formatVersion_,
        compressionType,
        inOutRecordSize,
        recordSize,
        static_cast<uint32_t>(bufferUsedSize_));
    WRITE_OR_LOG_AND_RETURN(file, &recordHeader, sizeof(recordHeader));
    WRITE_OR_LOG_AND_RETURN(file, compressor.getData(), compressedSize);
    inOutRecordSize = recordSize;
  } else {
    uint32_t recordSize = static_cast<uint32_t>(sizeof(FileFormat::RecordHeader) + bufferUsedSize_);
    FileFormat::RecordHeader recordHeader(
        getRecordType(),
        streamId,
        timestamp_,
        formatVersion_,
        CompressionType::None,
        inOutRecordSize,
        recordSize,
        0);
    WRITE_OR_LOG_AND_RETURN(file, &recordHeader, sizeof(recordHeader));
    WRITE_OR_LOG_AND_RETURN(file, buffer_.data(), bufferUsedSize_);
    inOutRecordSize = recordSize;
  }
  return 0;
}

const char* Record::typeName(Type type) {
  return RecordTypeConverter::toString(type);
}

Record::Type Record::nameToType(const std::string& name) {
  return toEnum<Record::Type>(name);
}

} // namespace vrs
