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

#include "RecordReaders.h"

#define DEFAULT_LOG_CHANNEL "VRSRecordReaders"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Throttler.h>

#include "ErrorCode.h"
#include "FileHandler.h"
#include "Record.h"

namespace {

vrs::utils::Throttler& getThrottler() {
  static vrs::utils::Throttler sThrottler{20 /*instances*/, 10 /*seconds*/};
  return sThrottler;
}

} // namespace

namespace vrs {

RecordReader::~RecordReader() = default;

RecordReader* RecordReader::init(FileHandler& file, uint32_t diskSize, uint32_t expandedSize) {
  file_ = &file;
  remainingDiskBytes_ = diskSize;
  remainingUncompressedSize_ = expandedSize;
  return this;
}

int64_t RecordReader::getFileOffset() const {
  return file_->getPos();
}

int UncompressedRecordReader::read(DataReference& destination, uint32_t& outReadSize) {
  outReadSize = 0;
  if (remainingUncompressedSize_ < destination.getSize()) {
    THROTTLED_LOGE(
        file_,
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

CompressionType UncompressedRecordReader::getCompressionType() const {
  return CompressionType::None;
}

void CompressedRecordReader::initCompressionType(CompressionType compressionType) {
  decompressor_.setCompressionType(compressionType);
}

int CompressedRecordReader::read(DataReference& destination, uint32_t& outReadSize) {
  outReadSize = 0;
  if (remainingUncompressedSize_ < destination.getSize()) {
    THROTTLED_LOGE(
        file_,
        "Tried to read {} bytes when at most {} are available.",
        destination.getSize(),
        remainingUncompressedSize_);
    return NOT_ENOUGH_DATA;
  }
  if (destination.getDataPtr1() != nullptr && destination.getDataSize1() > 0) {
    IF_ERROR_LOG_AND_RETURN(read(
        destination.getDataPtr1(), destination.getDataSize1(), destination.getSize(), outReadSize));
  }
  if (destination.getDataPtr2() != nullptr && destination.getDataSize2() > 0) {
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
    bool readData = false;
    if (decompressor_.getRemainingCompressedDataBufferSize() == 0 && remainingDiskBytes_ > 0) {
      size_t targetReadSize = (knownNeedSize >= remainingUncompressedSize_ + outReadSize)
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
      if (error != 0) {
        return error;
      }
      uint32_t readSize = static_cast<uint32_t>(file_->getLastRWSize());
      if (!XR_VERIFY(remainingDiskBytes_ >= readSize)) {
        return VRSERROR_INTERNAL_ERROR;
      }
      remainingDiskBytes_ -= readSize;
      readData = true;
    }
    uint32_t decompressedSize = 0;
    int error = decompressor_.decompress(
        reinterpret_cast<uint8_t*>(dest) + outReadSize, destSize - outReadSize, decompressedSize);
    outReadSize += decompressedSize;
    remainingUncompressedSize_ -= decompressedSize;
    if (error != 0) {
      return error;
    }
    if (!readData && decompressedSize == 0) {
      return NOT_ENOUGH_DATA;
    }
  } while (outReadSize < destSize);
  return 0;
}

void CompressedRecordReader::finish() {
  decompressor_.reset();
  RecordReader::finish();
}

CompressionType CompressedRecordReader::getCompressionType() const {
  return decompressor_.getCompressionType();
}

} // namespace vrs
