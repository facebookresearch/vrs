// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "RecordReaders.h"

#define DEFAULT_LOG_CHANNEL "VRSRecordReaders"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include "ErrorCode.h"
#include "FileHandler.h"

namespace vrs {

RecordReader::~RecordReader() = default;

RecordReader* RecordReader::init(FileHandler& file, uint32_t diskSize, uint32_t expandedSize) {
  file_ = &file;
  remainingDiskBytes_ = diskSize;
  remainingUncompressedSize_ = expandedSize;
  return this;
}

int UncompressedRecordReader::read(DataReference& destination, uint32_t& outReadSize) {
  outReadSize = 0;
  if (remainingUncompressedSize_ < destination.getSize()) {
    XR_LOGE(
        "Tried to read {} bytes when at most {} are available.",
        destination.getSize(),
        remainingUncompressedSize_);
    return NOT_ENOUGH_DATA;
  }
  int error = destination.readFrom(*file_, outReadSize);
  remainingDiskBytes_ -= outReadSize;
  remainingUncompressedSize_ -= outReadSize;
  return error;
}

void CompressedRecordReader::initCompressionType(CompressionType compressionType) {
  decompressor_.setCompressionType(compressionType);
}

int CompressedRecordReader::read(DataReference& destination, uint32_t& outReadSize) {
  outReadSize = 0;
  if (remainingUncompressedSize_ < destination.getSize()) {
    XR_LOGE(
        "Tried to read {} bytes when at most {} are available.",
        destination.getSize(),
        remainingUncompressedSize_);
    return NOT_ENOUGH_DATA;
  }
  if (destination.getDataPtr1() && destination.getDataSize1() > 0) {
    IF_ERROR_LOG_AND_RETURN(read(
        destination.getDataPtr1(), destination.getDataSize1(), destination.getSize(), outReadSize));
  }
  if (destination.getDataPtr2() && destination.getDataSize2() > 0) {
    uint32_t outReadSize2 = 0;
    IF_ERROR_LOG_AND_RETURN(
        read(destination.getDataPtr2(), destination.getDataSize2(), outReadSize2, outReadSize2));
    outReadSize += outReadSize2;
  }
  return 0;
}

int CompressedRecordReader::read(
    void* dest,
    uint32_t destSize,
    uint32_t knownNeedSize,
    uint32_t& outReadSize) {
  outReadSize = 0;
  do {
    if (decompressor_.getRemainingCompressedDataBufferSize() == 0 && remainingDiskBytes_ > 0) {
      size_t targetReadSize = (knownNeedSize - outReadSize >= remainingUncompressedSize_)
          ? remainingDiskBytes_
          : knownNeedSize;
      size_t recommendedNextBufferSize = decompressor_.getRecommendedInputBufferSize();
      if (targetReadSize < recommendedNextBufferSize) {
        targetReadSize = recommendedNextBufferSize;
      }
      if (targetReadSize > remainingDiskBytes_) {
        targetReadSize = remainingDiskBytes_;
      }
      int error =
          file_->read(decompressor_.allocateCompressedDataBuffer(targetReadSize), targetReadSize);
      remainingDiskBytes_ -= static_cast<uint32_t>(file_->getLastRWSize());
      if (error != 0) {
        return error;
      }
    }
    uint32_t decompressedSize = 0;
    int error = decompressor_.decompress(
        reinterpret_cast<uint8_t*>(dest) + outReadSize, destSize - outReadSize, decompressedSize);
    outReadSize += decompressedSize;
    remainingUncompressedSize_ -= decompressedSize;
    if (error != 0) {
      return error;
    }
  } while (outReadSize < destSize);
  return 0;
}

void CompressedRecordReader::finish() {
  decompressor_.reset();
  RecordReader::finish();
}

} // namespace vrs
