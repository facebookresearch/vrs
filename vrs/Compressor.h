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
#include <string>
#include <vector>

#include "ForwardDefinitions.h"
#include "WriteFileHandler.h"

namespace vrs {

/// \brief VRS Compression setting.
///
/// VRS records are compressed by default, using the LZ4_FAST setting, which is an extremely fast
/// lossless compression.
///
/// VRS compression is completely transparent: records are compressed and uncompressed without
/// the need to do anything when writing or reading data. Record sizes are always showing the
/// uncompressed size.
///
/// VRS files can easily be recompressed offline using VRStool.
enum class CompressionPreset {
  Undefined = -1, ///< when not set explicitly
  None = 0, ///< No compression
  Lz4Fast, ///< Fast compression speed, very fast decompression, not great compression ratio.
  Lz4Tight, ///< Slower compression speed, very fast decompression, better compression ratio.

  ZstdFast,
  ZstdLight,
  ZstdMedium,
  ZstdHeavy,
  ZstdHigh,
  ZstdTight,
  ZstdMax,

  COUNT,

  FirstLz4Preset = Lz4Fast,
  LastLz4Preset = Lz4Tight,

  FirstZstdPreset = ZstdFast,
  LastZstdPreset = ZstdMax,

  CompressedFirst = Lz4Fast,
  CompressedLast = ZstdMax,

  Default = Lz4Fast ///< Default preset

}; // namespace vrs

std::string toPrettyName(CompressionPreset preset);
std::string toString(CompressionPreset preset);

template <>
CompressionPreset toEnum<CompressionPreset>(const std::string& presetName);

/// \brief Helper class to compress data using lz4 or zstd presets.
///
/// You can switch between presets at no cost. If presets don't work well enough with your data, you
/// can easily experiment with new settings and add your own preset to CompressionPreset.
/// @internal
class Compressor {
 public:
  static const size_t kMinByteCountForCompression; // don't try to compress small payloads

  Compressor();
  ~Compressor();

  /// Compress some data using a specific preset.
  /// If the compression setting doesn't lead to a smaller payload, because the data can't be
  /// compressed, then the record won't be compressed at all.
  /// @param data: Pointer to the data to compress.
  /// @param dataSize: Number of bytes in the buffer to compress.
  /// @param preset: Compression preset to use.
  /// @param headerSpace: Number of bytes to reserve at the beginning of the buffer for a header
  /// initialized manually later.
  /// @return The number of bytes of compressed data, or 0 in case of failure.
  uint32_t
  compress(const void* data, size_t dataSize, CompressionPreset preset, size_t headerSpace = 0);

  /// Frame compression APIs, with streaming to a file.
  /// Write to a file a block of data (a "frame") to be compressed. That data will be logically self
  /// contained, and its size will be retrievable when decoding the first byte. The frame can be
  /// added in multiple calls, making it easy to write a lot of data without needing large
  /// intermediate buffers.
  ///
  /// Start a new frame, declaring its full size upfront. This size must be respected.
  /// @param frameSize: exact number of bytes that will be added to the frame in total.
  /// @param zstdPreset: compression preset to use. Only zstd presets are supported by this API.
  /// @param outSize: total number of compressed bytes written.
  /// @return 0 for success, or an error code. This call initializes outSize to 0.
  int startFrame(size_t frameSize, CompressionPreset zstdPreset, uint32_t& outSize);
  /// Add data to a frame started before.
  /// The total amount of data may not exceed the declared size of the frame when it was started.
  /// @param file: A file open for writing.
  /// @param data: A pointer to the data to add to the frame.
  /// @param dataSize: Number of bytes to add to the frame.
  /// @param inOutCompressedSize: Updated number of bytes written out to the file.
  /// @param maxCompressedSize: Max number of bytes the compressed record may get. Fail if the
  /// compressed data is larger, while guarantying that fewer bytes have been written to disk.
  /// @return 0 for success, or an error code. The input buffer may be recycled immediately, but
  /// all the data may not have been written out to the file yet. You may add all the data of the
  /// frame in as many calls as you wish, one byte at a time, all at once, and anything in between.
  int addFrameData(
      WriteFileHandler& file,
      const void* data,
      size_t dataSize,
      uint32_t& inOutCompressedSize,
      size_t maxCompressedSize = 0);
  /// Write out all the data left in internal compression buffers to disk, and complete the frame.
  /// After this call has been made, a new frame maybe started.
  /// @param file: A file open for writing.
  /// @param inOutCompressedSize: Updated number of bytes written out to the file.
  /// @param maxCompressedSize: Max number of bytes the compressed record may get. Fail if the
  /// compressed data is larger, while guarantying that fewer bytes have been written to disk.
  /// @return 0 for success, or an error code. After this call, inOutSize counts the total number of
  /// bytes used for the compressed frame, including compression metadata. This number should be
  /// smaller than the frame size, but might be slightly larger, if the data couldn't be compressed.
  int endFrame(WriteFileHandler& file, uint32_t& inOutCompressedSize, size_t maxCompressedSize = 0);

  /// Get the compressed data after compression. The size to consider was returned by compress().
  /// @return Pointer to the compressed data.
  const void* getData() const {
    return buffer_.data();
  }
  /// Get the space reserved for a header.
  template <class HeaderType>
  HeaderType* getHeader() {
    return reinterpret_cast<HeaderType*>(buffer_.data());
  }
  CompressionType getCompressionType() const;

  /// Really deallocate the buffer's memory (clear() doesn't do that)
  void clear() {
    std::vector<uninitialized_byte> blank;
    buffer_.swap(blank);
  }

  static bool shouldTryToCompress(CompressionPreset preset, size_t size);

  struct uninitialized_byte final {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init, modernize-use-equals-default)
    uninitialized_byte() {} // do not use '= default' as it will initialize byte!
    uint8_t byte;
  };

 private:
  class CompressorImpl;
  std::unique_ptr<CompressorImpl> impl_;
  std::vector<uninitialized_byte> buffer_;
};

} // namespace vrs
