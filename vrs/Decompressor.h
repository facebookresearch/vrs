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

#include <vrs/ForwardDefinitions.h>
#include <vrs/VrsExport.h>
#include <vrs/helpers/IOVector.h>

namespace vrs {

/// \brief Decompressor helper class that decodes data into a caller-provided buffer.
///
/// The data to decompress must be in the internal buffer. Use allocateCompressedDataBuffer(size)
/// to reserve space and get a pointer to fill, for example by reading directly from disk.
/// Call decompress() with a destination and its size to decode into that buffer. The caller can
/// decode a record in parts instead of allocating a separate buffer for the entire payload.
/// @internal For VRS internal use only.
class VRS_API Decompressor {
 public:
  Decompressor();
  ~Decompressor();

  void setCompressionType(CompressionType compressionType);
  CompressionType getCompressionType() const {
    return compressionType_;
  }

  size_t getRecommendedInputBufferSize() const;

  /// Decompress a maximum of destinationSize bytes of data, at the pointed to destination,
  /// and return the number of bytes read in outReadSize.
  /// @return 0 if no error occurred, a file system error code otherwise.
  int decompress(void* destination, uint32_t destinationSize, uint32_t& outReadSize);

  /// Get a buffer of a specific size where to write the data to be decoded.
  /// The decompressor will assume that the buffer will be filed to the requested size.
  /// @param requestSize: Size to allocate.
  /// @return Pointer to the buffer.
  /// @internal
  void* allocateCompressedDataBuffer(size_t requestSize);

  /// Tell how many bytes we haven't yet decoded from our compressed data buffer.
  /// @return Number of bytes.
  /// @internal
  size_t getRemainingCompressedDataBufferSize() const {
    return readSize_ - decodedSize_;
  }

  /// Frame compression APIs, to read compressed frames from a file.
  /// 1/ call reset() (if the compressor was used before).
  /// 2/ call initFrame to know how large the uncompressed frame is.
  /// 3/ allocate enough memory for the whole frame.
  /// 4/ read the whole frame in the buffer
  /// 5/ if reading more than one successive frames, *do not call reset*, go straight back to 2/
  ///
  /// Start to read a frame, by sniffing the upcoming frame's size.
  /// @param file: A file open for reading, at the position of the frame.
  /// @param frameSize: On success, set to the size in bytes of the upcoming frame.
  /// @param maxReadSize: Max number of bytes of compressed frames data.
  /// If there are multiple frames, or the size of the compressed frame(s) is unknown, this number
  /// maybe the remaining file size. This number is updated as needed, if data is read from the file
  /// and placed in an internal decode buffer.
  /// @return 0 for success, or an error code. This could happen if you try to read data that isn't
  /// a compressed frame. Note that the file might be read past the end of the compressed frame, but
  /// not beyond the incoming value of inOutMaxReadSize. So when reading successive frames, do not
  /// reset the compression object between frames. Instead, just call initFrame again, after
  /// successfully reading a frame.
  int initFrame(FileHandler& file, size_t& outFrameSize, size_t& inOutMaxReadSize);
  /// Read a compressed frame, all at once.
  /// @param file: A file open for reading, at the position of the frame.
  /// @param dst: A pointer to an allocated buffer large enough for the whole frame.
  /// @param frameSize: The size of the frame, as returned by initFrame. The buffer must be large
  /// enough to read the whole frame at once.
  /// @param maxReadSize: Max number of bytes of compressed frames data left.
  int readFrame(FileHandler& file, void* dst, size_t frameSize, size_t& inOutMaxReadSize);

  /// Forget any remaining compressed data, get ready for a new frame.
  void reset();

 private:
  const void* getCompressedData() const {
    return compressedBuffer_.data() + decodedSize_;
  }
  size_t getCompressedDataSize() const {
    return readSize_ - decodedSize_;
  }

  class Lz4Decompressor;
  std::unique_ptr<Lz4Decompressor> lz4Context_;
  class ZstdDecompressor;
  std::unique_ptr<ZstdDecompressor> zstdContext_;
  helpers::IOVector<uint8_t> compressedBuffer_;
  CompressionType compressionType_{};
  size_t readSize_ = {};
  size_t decodedSize_ = {};
  size_t lastResult_ = {};
};

} // namespace vrs
