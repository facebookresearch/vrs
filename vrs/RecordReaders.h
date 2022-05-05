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

#include "DataReference.h"
#include "Decompressor.h"
#include "ErrorCode.h"

namespace vrs {

class FileHandler;

/// \brief Abstract VRS internal helper class to read & (if necessary) uncompress records.
/// @internal
class RecordReader {
 public:
  virtual ~RecordReader();

  /// Initialize the RecordReader to read a record from a file.
  /// @param file: File to read from, ready to read the record at the current file position.
  /// @param diskSize: Size of the record on disk.
  /// @param expandedSize: Size of the record uncompressed.
  /// @return Pointer to the reader.
  RecordReader* init(FileHandler& file, uint32_t diskSize, uint32_t expandedSize);

  /// Read data to a DataReference.
  /// @param destination: DataReference to read data to.
  /// @param outReadSize: Reference to set to the number of bytes read.
  /// @return 0 on success, or a non-zero error code.
  virtual int read(DataReference& destination, uint32_t& outReadSize) = 0;

  /// Fill-up a buffer. Might reduce the buffer's size, if not enough data was available.
  /// @param buffer: Reference to a buffer of bytes to fill-up.
  /// @return 0 on success, or a non-zero error code.
  template <typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
  int read(std::vector<T>& buffer) {
    DataReference bufferDataReference(buffer);
    uint32_t readSize = 0;
    int res = read(bufferDataReference, readSize);
    buffer.resize(readSize / sizeof(T));
    return res;
  }

  /// Read a number of bytes to the destination buffer. If the record is compressed, the data
  /// is uncompressed.
  /// @param destination: Pointer to the buffer where to write the read data.
  /// @param size: Number of bytes to read.
  /// @return 0 on success, or a non-zero error code.
  int read(void* destination, size_t size) {
    DataReference bufferDataReference(destination, static_cast<uint32_t>(size));
    uint32_t readSize = 0;
    const int res = read(bufferDataReference, readSize);
    if (res != 0) {
      return res;
    }
    return readSize == size ? 0 : READ_ERROR;
  }

  /// Discard any unread data.
  virtual void finish() {}

  /// Tell how many bytes of record data haven't been read/consummed yet.
  /// @return Number of unread bytes (uncompressed).
  uint32_t getUnreadBytes() const {
    return remainingUncompressedSize_;
  }

 protected:
  FileHandler* file_;
  uint32_t remainingDiskBytes_;
  uint32_t remainingUncompressedSize_;
};

/// \brief RecordReader specialized to read uncompressed records. For VRS internal usage only.
/// @internal
class UncompressedRecordReader : public RecordReader {
 public:
  int read(DataReference& destination, uint32_t& outReadSize) override;
};

/// \brief RecordReader specialized to read compressed records. For VRS internal usage only.
/// @internal
class CompressedRecordReader : public RecordReader {
 public:
  void initCompressionType(CompressionType compressionType);
  int read(DataReference& destination, uint32_t& outReadSize) override;
  void finish() override;

 private:
  int read(void* dest, uint32_t destSize, uint32_t overalSize, uint32_t& outReadSize);

  Decompressor decompressor_;
};

} // namespace vrs
