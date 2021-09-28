// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "Decompressor.h"

#include <cstring>

#include <algorithm>
#include <vector>

#include <lz4frame.h>
#include <zstd.h>

#define DEFAULT_LOG_CHANNEL "VRSDecompressor"
#include <logging/Checks.h>
#include <logging/Log.h>

#include <vrs/helpers/FileMacros.h>
#include "ErrorCode.h"
#include "FileHandler.h"
#include "Record.h"

namespace vrs {

static size_t kMaxInputBufferSize = 2 * 1024 * 1024; // 2 MB
static size_t kMinInputBufferSize = 4 * 1024;

class Decompressor::Lz4Decompressor {
 public:
  Lz4Decompressor() {
    LZ4F_createDecompressionContext(&context_, LZ4F_VERSION);
  }
  ~Lz4Decompressor() {
    LZ4F_freeDecompressionContext(context_);
  }
  size_t
  decompress(void* dstBuffer, size_t* dstSizePtr, const void* srcBuffer, size_t* srcSizePtr) {
    return LZ4F_decompress(context_, dstBuffer, dstSizePtr, srcBuffer, srcSizePtr, &options_);
  }
  void resetContext() {
    LZ4F_resetDecompressionContext(context_);
  }

 private:
  LZ4F_decompressionContext_t context_ = {};
  LZ4F_decompressOptions_t options_ = {};
};

class Decompressor::ZstdDecompressor {
 public:
  ZstdDecompressor() {
    context_ = ZSTD_createDStream();
  }
  ~ZstdDecompressor() {
    ZSTD_freeDStream(context_);
  }
  inline size_t decompress(
      const void* compressedData, // our compressed data buffer
      size_t& inOutdecodedSize, // number of bytes decoded so far, to be updated
      size_t compressedDataSize, // number of bytes in the compressedData buffer
      void* destination, // where to write decoded data
      uint32_t destinationSize, // how many bytes to read (max)
      uint32_t& outWrittenSize) // how many bytes were written {
  {
    ZSTD_inBuffer input = {compressedData, compressedDataSize, inOutdecodedSize};
    ZSTD_outBuffer output = {destination, destinationSize, 0};
    size_t result = ZSTD_decompressStream(context_, &output, &input);
    outWrittenSize = static_cast<uint32_t>(output.pos);
    inOutdecodedSize = input.pos;
    return result;
  }
  void resetContext() {
    ZSTD_initDStream(context_);
  }

  ZSTD_DStream* getContext() const {
    return context_;
  }

 private:
  ZSTD_DStream* context_;
};

void* Decompressor::allocateCompressedDataBuffer(size_t requestSize) {
  XR_CHECK_LE(decodedSize_, readSize_);
  // see if there are bytes in the buffer that we need to preserve
  if (readSize_ == decodedSize_) {
    // nothing to preserve, just make sure our buffer is big enough & use from the beginning
    if (requestSize > compressedBuffer_.size()) {
      compressedBuffer_.resize(std::max(kMinInputBufferSize, requestSize));
    }
    decodedSize_ = 0;
    readSize_ = requestSize;
    return compressedBuffer_.data();
  } else if (readSize_ + requestSize > compressedBuffer_.size()) {
    // our read size exceeds what we have left between our last read & the end of the buffer
    size_t undecodedSize = readSize_ - decodedSize_;
    if (undecodedSize + requestSize > compressedBuffer_.size()) {
      // the buffer is just too small: we need a new one
      std::vector<uint8_t> newBuffer;
      newBuffer.resize(undecodedSize + requestSize);
      if (undecodedSize > 0) {
        memcpy(newBuffer.data(), compressedBuffer_.data() + decodedSize_, undecodedSize);
      }
      compressedBuffer_.swap(newBuffer);
    } else {
      // we can fit everything in the buffer, but we need to shift the undecoded data to make room
      memmove(compressedBuffer_.data(), compressedBuffer_.data() + decodedSize_, undecodedSize);
    }
    // in both cases, we've moved the data to preserve at the beginning of the buffer
    decodedSize_ = 0;
    readSize_ = undecodedSize + requestSize;
    return compressedBuffer_.data() + undecodedSize;
  } else {
    // The new requests fits in the current buffer: we put it after the last read data
    size_t previousReadSize = readSize_;
    readSize_ += requestSize;
    return compressedBuffer_.data() + previousReadSize;
  }
}

Decompressor::Decompressor() {
  lz4Context_ = std::make_unique<Lz4Decompressor>();
  zstdContext_ = std::make_unique<ZstdDecompressor>();
}

void Decompressor::setCompressionType(CompressionType compressionType) {
  compressionType_ = compressionType;
}

size_t Decompressor::getRecommendedInputBufferSize() {
  return std::max(std::min(lastResult_, kMaxInputBufferSize), kMinInputBufferSize);
}

Decompressor::~Decompressor() = default;

void Decompressor::reset() {
  if (compressionType_ == CompressionType::Lz4) {
    if (lastResult_) {
      lz4Context_->resetContext();
    }
  } else if (compressionType_ == CompressionType::Zstd) {
    zstdContext_->resetContext();
  }
  compressionType_ = CompressionType::None;
  readSize_ = 0;
  decodedSize_ = 0;
  lastResult_ = 0;
}

#define DECOMPRESSION_ERROR(code__, name__) \
  domainErrorCode(ErrorDomain::ZstdDecompressionErrorDomain, code__, name__)

#define IF_DECOMP_ERROR_LOG_AND_RETURN(operation__)                   \
  do {                                                                \
    zresult = operation__;                                            \
    if (ZSTD_isError(zresult)) {                                      \
      const char* errorName = ZSTD_getErrorName(zresult);             \
      XR_LOGE("{} failed: {}, {}", #operation__, zresult, errorName); \
      return DECOMPRESSION_ERROR(zresult, errorName);                 \
    }                                                                 \
  } while (false)

#define READ_OR_LOG_AND_RETURN(size__)                                                             \
  do {                                                                                             \
    size_t readSize__ = std::min(std::min<size_t>(size__, inOutMaxReadSize), kMaxInputBufferSize); \
    IF_ERROR_LOG_AND_RETURN(file.read(allocateCompressedDataBuffer(readSize__), readSize__));      \
    inOutMaxReadSize -= readSize__;                                                                \
  } while (false)

int Decompressor::initFrame(FileHandler& file, size_t& outFrameSize, size_t& inOutMaxReadSize) {
  const size_t ZSTD_frameHeaderSize_max = 256;
  if (getCompressedDataSize() < ZSTD_frameHeaderSize_max) {
    READ_OR_LOG_AND_RETURN(ZSTD_frameHeaderSize_max - getCompressedDataSize());
  }
  unsigned long long sz = ZSTD_getFrameContentSize(getCompressedData(), getCompressedDataSize());
  if (sz == ZSTD_CONTENTSIZE_UNKNOWN) {
    return DECOMPRESSION_ERROR(lastResult_, "Unknown frame size");
  } else if (sz == ZSTD_CONTENTSIZE_ERROR) {
    return DECOMPRESSION_ERROR(lastResult_, "Bad content size");
  }
  outFrameSize = static_cast<size_t>(sz);
  return 0;
}

int Decompressor::readFrame(
    FileHandler& file,
    void* dst,
    size_t frameSize,
    size_t& inOutMaxReadSize) {
  size_t zresult;
  IF_DECOMP_ERROR_LOG_AND_RETURN(ZSTD_initDStream(zstdContext_->getContext()));
  if (getCompressedDataSize() < zresult) {
    READ_OR_LOG_AND_RETURN(zresult - getCompressedDataSize());
  }
  ZSTD_outBuffer output{dst, frameSize, 0};
  do {
    if (getCompressedDataSize() == 0 && zresult > 0) {
      if (inOutMaxReadSize == 0) {
        XR_LOGW("Decompression error: {} more input bytes needed", zresult);
        return NOT_ENOUGH_DATA;
      }
      READ_OR_LOG_AND_RETURN(zresult);
    }
    ZSTD_inBuffer input{compressedBuffer_.data(), readSize_, decodedSize_};
    IF_DECOMP_ERROR_LOG_AND_RETURN(
        ZSTD_decompressStream(zstdContext_->getContext(), &output, &input));
    decodedSize_ = input.pos;
  } while (zresult > 0);
  return 0;
}

int Decompressor::decompress(void* destination, uint32_t destinationSize, uint32_t& outReadSize) {
  if (compressionType_ == CompressionType::Lz4) {
    size_t writtenSize = destinationSize;
    size_t sourceSize = readSize_ - decodedSize_;
    lastResult_ = lz4Context_->decompress(
        destination, &writtenSize, compressedBuffer_.data() + decodedSize_, &sourceSize);
    if (LZ4F_isError(lastResult_)) {
      XR_LOGE("Decompression error {}", LZ4F_getErrorName(lastResult_));
      return domainErrorCode(
          ErrorDomain::Lz4DecompressionErrorDomain, lastResult_, LZ4F_getErrorName(lastResult_));
    }
    decodedSize_ += sourceSize;
    outReadSize = static_cast<uint32_t>(writtenSize);
  } else if (compressionType_ == CompressionType::Zstd) {
    lastResult_ = zstdContext_->decompress(
        compressedBuffer_.data(),
        decodedSize_,
        readSize_,
        destination,
        destinationSize,
        outReadSize);
    if (ZSTD_isError(lastResult_)) {
      XR_LOGE("Decompression error {}", ZSTD_getErrorName(lastResult_));
      return domainErrorCode(
          ErrorDomain::ZstdDecompressionErrorDomain, lastResult_, ZSTD_getErrorName(lastResult_));
    }
  }
  return 0;
}

} // namespace vrs
