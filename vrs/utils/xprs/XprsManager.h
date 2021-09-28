// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <array>

#include <xprs.h>

#include <vrs/DataLayout.h>
#include <vrs/utils/CopyRecords.h>

namespace vrs::vxprs {

struct EncoderOptions;

// Helper class to track record block data in a set or a map
struct BlockId {
  BlockId() : recordType{Record::Type::UNDEFINED}, formatVersion{0}, blockIndex{0} {}
  BlockId(const CurrentRecord& record, size_t _blockIndex)
      : recordType{record.recordType},
        formatVersion{record.formatVersion},
        blockIndex{static_cast<uint32_t>(_blockIndex)} {}
  BlockId(Record::Type _recordType, uint32_t _formatVersion, size_t _blockIndex)
      : recordType{_recordType},
        formatVersion{_formatVersion},
        blockIndex{static_cast<uint32_t>(_blockIndex)} {}
  BlockId(const BlockId& other)
      : recordType{other.recordType},
        formatVersion{other.formatVersion},
        blockIndex{other.blockIndex} {}

  void clear() {
    recordType = Record::Type::UNDEFINED;
    formatVersion = 0;
    blockIndex = 0;
  }
  bool isValid() const {
    return recordType != Record::Type::UNDEFINED;
  }

  bool operator<(const BlockId& rhs) const {
    return recordType < rhs.recordType ||
        (recordType == rhs.recordType &&
         (formatVersion < rhs.formatVersion ||
          (formatVersion == rhs.formatVersion && blockIndex < rhs.blockIndex)));
  }
  bool operator==(const BlockId& rhs) const {
    return recordType == rhs.recordType && formatVersion == rhs.formatVersion &&
        blockIndex == rhs.blockIndex;
  }
  bool operator!=(const BlockId& rhs) const {
    return !operator==(rhs);
  }
  BlockId& operator=(const BlockId& rhs) {
    recordType = rhs.recordType;
    formatVersion = rhs.formatVersion;
    blockIndex = rhs.blockIndex;
    return *this;
  };

  bool isRightBefore(const BlockId& rhs) const {
    return recordType == rhs.recordType && formatVersion == rhs.formatVersion &&
        blockIndex + 1 == rhs.blockIndex;
  }

  string asString() const {
    return toString(recordType) + " v" + std::to_string(formatVersion) + " @" +
        std::to_string(blockIndex);
  }

  Record::Type recordType;
  uint32_t formatVersion;
  uint32_t blockIndex;
};

bool isCompressCandidate(
    RecordFileReader& reader,
    StreamId id,
    const EncoderOptions& encoderOptions,
    BlockId& imageSpecBlock,
    BlockId& pixelBlock);

/// Make stream filter function that uses the provided encoder options to select which stream
/// to encode during a vrs::utils::copyRecords() operation, for the encoding of streams.
/// @param: encoderOptions used to determine which of the streams can be encoded, and during
/// video encoding.
/// @return A MakeStreamFilterFunction designed to be used by vrs::utils::copyRecords().
vrs::utils::MakeStreamFilterFunction makeStreamFilter(const EncoderOptions& encoderOptions);

std::array<uint32_t, 256> getHistogram(const uint8_t* buffer, size_t length);
inline std::array<uint32_t, 256> getHistogram(const vector<uint8_t> buffer) {
  return getHistogram(buffer.data(), buffer.size());
}
void printHistogram(const std::array<uint32_t, 256>& histogram);

} // namespace vrs::vxprs
