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

#pragma once

#include <memory>

#include <vrs/FileHandler.h>
#include <vrs/Record.h>
#include <vrs/RecordReaders.h>

namespace vrs::utils {

/// A FileHandler that reads data from a buffer
class BufferFileHandler : public FileHandler {
 public:
  BufferFileHandler() : fileHandlerName_{"BufferFileHandler"} {}
  template <class T>
  explicit BufferFileHandler(const vector<T>& buffer) {
    init(buffer);
  }

  template <class T>
  inline void init(const vector<T>& buffer) {
    init(buffer.data(), buffer.size() * sizeof(T));
  }
  void init(const void* data, size_t length) {
    data_ = reinterpret_cast<const uint8_t*>(data);
    readPos_ = 0;
    totalSize_ = length;
    lastReadSize_ = 0;
    lastError_ = 0;
  }

  std::unique_ptr<FileHandler> makeNew() const override {
    return std::make_unique<BufferFileHandler>();
  }
  const string& getFileHandlerName() const override {
    return fileHandlerName_;
  }

  int openSpec(const FileSpec& /*fileSpec*/) override {
    return data_ != nullptr ? 0 : FAILURE;
  }
  bool isOpened() const override {
    return data_ != nullptr;
  }
  int64_t getTotalSize() const override {
    return totalSize_;
  }
  int close() override {
    data_ = nullptr;
    readPos_ = 0;
    totalSize_ = 0;
    lastError_ = 0;
    return 0;
  }
  inline int status(int returnStatus) {
    lastError_ = returnStatus;
    return lastError_;
  }
  int skipForward(int64_t offset) override {
    if (readPos_ + offset > totalSize_ || readPos_ + offset < 0) {
      return status(FAILURE);
    }
    readPos_ += offset;
    return status(0);
  }
  int setPos(int64_t offset) override {
    if (offset < 0 || offset > totalSize_) {
      return status(FAILURE);
    }
    readPos_ = offset;
    return status(0);
  }
  int read(void* buffer, size_t length) override {
    if (readPos_ + length > totalSize_) {
      return status(FAILURE);
    }
    memcpy(buffer, data_ + readPos_, length);
    readPos_ += length;
    lastReadSize_ = length;
    return status(0);
  }
  size_t getLastRWSize() const override {
    return lastReadSize_;
  }
  bool isReadOnly() const override {
    return true;
  }
  vector<std::pair<string, int64_t>> getFileChunks() const override {
    return {{"memory_buffer", totalSize_}};
  }
  void forgetFurtherChunks(int64_t maxSize) override {}
  int getLastError() const override {
    return lastError_;
  }
  bool isEof() const override {
    return readPos_ >= totalSize_;
  }
  int64_t getPos() const override {
    return readPos_;
  }
  int64_t getChunkPos() const override {
    return readPos_;
  }
  int getChunkRange(int64_t& outChunkOffset, int64_t& outChunkSize) const override {
    outChunkOffset = 0;
    outChunkSize = totalSize_;
    return 0;
  }

 private:
  const string fileHandlerName_;
  const uint8_t* data_{};
  int64_t totalSize_{0};
  int64_t readPos_{0};
  size_t lastReadSize_{0};
  int lastError_{0};
};

/// A RecordReader that reads data from a buffer
/// Useful for video decoding in a background thread
class BufferReader : public RecordReader {
 public:
  RecordReader* init(const vector<uint8_t>& buffer) {
    bufferReader_.init(buffer);
    uint32_t bufferSize = static_cast<uint32_t>(buffer.size());
    return RecordReader::init(bufferReader_, bufferSize, bufferSize);
  }

  /// Read data to a DataReference.
  /// @param destination: DataReference to read data to.
  /// @param outReadSize: Reference to set to the number of bytes read.
  /// @return 0 on success, or a non-zero error code.
  int read(DataReference& destination, uint32_t& outReadSize) override {
    int status = destination.readFrom(bufferReader_, outReadSize);
    remainingDiskBytes_ -= outReadSize;
    remainingUncompressedSize_ -= outReadSize;
    return status;
  }

  CompressionType getCompressionType() const override {
    return CompressionType::None;
  }

 protected:
  BufferFileHandler bufferReader_;
};

} // namespace vrs::utils
