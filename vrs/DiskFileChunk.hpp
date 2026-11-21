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

#include <cerrno>
#include <cstdio>

#include <vrs/ErrorCode.h>
#include <vrs/FileHandler.h>
#include <vrs/helpers/Strings.h>
#include <vrs/os/CompilerAttributes.h>
#include <vrs/os/Utils.h>

namespace vrs {

class DiskFileChunk {
 public:
  explicit DiskFileChunk(size_t index) : index_{index} {}
  DiskFileChunk(size_t index, string path, int64_t offset, int64_t size)
      : index_{index}, path_{std::move(path)}, offset_{offset}, size_{size} {}
  DiskFileChunk(const DiskFileChunk& other) = delete;
  DiskFileChunk(DiskFileChunk&& other) = delete;
  ~DiskFileChunk() {
    close();
  }

  DiskFileChunk& operator=(const DiskFileChunk& other) = delete;
  DiskFileChunk& operator=(DiskFileChunk&& other) = delete;

  int create(const string& newPath, MAYBE_UNUSED const FileSpec::Extras& options) {
    close();
    file_ = os::fileOpen(newPath, "wb");
    if (file_ == nullptr) {
      return errno;
    }
    path_ = newPath;
    offset_ = 0;
    size_ = 0;
    uint64_t bufferSize = 0;
    if (helpers::getUInt64(options, "io_buffer_size", bufferSize)) {
      return setvbuf(file_, nullptr, bufferSize == 0 ? _IONBF : _IOFBF, bufferSize);
    }
#if IS_ANDROID_PLATFORM()
    const size_t kBufferingSize = 128 * 1024;
    return setvbuf(file_, nullptr, _IOFBF, kBufferingSize);
#else
    return SUCCESS;
#endif
  }
  int open(bool readOnly, MAYBE_UNUSED const FileSpec::Extras& options) {
    if (file_ != nullptr) {
      os::fileClose(file_);
    }
    file_ = os::fileOpen(path_, readOnly ? "rb" : "rb+");
    return file_ != nullptr ? SUCCESS : errno;
  }
  bool isOpened() const {
    return file_ != nullptr;
  }
  int rewind() const {
    // Unlike ::rewind(), fseek() reports a failure to flush pending writes.
    const int error = ::fseek(file_, 0, SEEK_SET) != 0 ? errno : SUCCESS;
    ::clearerr(file_);
    return error;
  }
  int flush() {
    return ::fflush(file_) != 0 ? errno : SUCCESS;
  }
  int tell(int64_t& outFilepos) const {
    outFilepos = os::fileTell(file_);
    return outFilepos < 0 ? errno : SUCCESS;
  }
  int seek(int64_t pos, int origin) {
    return os::fileSeek(file_, pos, origin) != 0 ? errno : SUCCESS;
  }
  int read(void* ptr, size_t bufferSize, size_t& outReadSize) const {
    outReadSize = ::fread(ptr, 1, bufferSize, file_);
    return bufferSize == outReadSize ? SUCCESS : ::ferror(file_) ? errno : DISKFILE_NOT_ENOUGH_DATA;
  }
  int write(const void* data, size_t dataSize, size_t& outWrittenSize) const {
    outWrittenSize = ::fwrite(data, 1, dataSize, file_);
    return dataSize == outWrittenSize ? SUCCESS
        : ::ferror(file_)             ? errno
                                      : DISKFILE_PARTIAL_WRITE_ERROR;
  }
  int truncate(int64_t newSize) {
    if (os::fileSetSize(file_, newSize) == 0) {
      size_ = newSize;
      return SUCCESS;
    }
    return errno;
  }
  int close() {
    int error = SUCCESS;
    if (file_ != nullptr) {
      error = os::fileClose(file_) != 0 ? errno : SUCCESS;
      file_ = nullptr;
    }
    return error;
  }
  bool eof() const {
    return ::feof(file_) != 0;
  }
  int64_t getOffset() const {
    return offset_;
  }
  void setOffset(int64_t newOffset) {
    offset_ = newOffset;
  }
  int64_t getSize() const {
    return size_;
  }
  void setSize(int64_t newSize) {
    size_ = newSize;
  }
  const string& getPath() const {
    return path_;
  }
  bool contains(int64_t fileOffset) const {
    return fileOffset >= offset_ && fileOffset < offset_ + size_;
  }
  size_t getIndex() const {
    return index_;
  }

 private:
  const size_t index_; // position of this chunk in the file's chunk list
  FILE* file_{}; // nullptr when closed
  string path_; // path of this chunk
  int64_t offset_{}; // offset of this chunk in the file
  int64_t size_{}; // size of the chunk
};

} // namespace vrs
