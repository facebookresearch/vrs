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

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <vrs/RecordFormat.h>
#include <vrs/utils/BufferRecordReader.hpp>
#include <vrs/utils/ImageLoader.h>
#include <vrs/utils/PixelFrame.h>

using namespace std;
using namespace vrs;
using namespace vrs::utils;

namespace {

// Helper to create a simple raw RGB8 image buffer with known pixel data.
// Returns a buffer of width*height*3 bytes with a simple gradient pattern.
vector<uint8_t> makeRawRgb8Buffer(uint32_t width, uint32_t height) {
  const uint32_t stride = width * 3;
  vector<uint8_t> buffer(stride * height);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      size_t idx = y * stride + x * 3;
      buffer[idx] = static_cast<uint8_t>(x & 0xFF);
      buffer[idx + 1] = static_cast<uint8_t>(y & 0xFF);
      buffer[idx + 2] = static_cast<uint8_t>((x + y) & 0xFF);
    }
  }
  return buffer;
}

// Helper to create a simple raw GREY16 image buffer with known pixel data.
// Returns a buffer of width*height*2 bytes with values that will show transformation.
vector<uint8_t> makeRawGrey16Buffer(uint32_t width, uint32_t height) {
  vector<uint8_t> buffer(width * height * 2);
  uint16_t* pixels = reinterpret_cast<uint16_t*>(buffer.data());
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      // Use values that span the 16-bit range to test downscaling
      // Values: 0, 256, 512, ... which when shifted right by 8 become 0, 1, 2, ...
      pixels[y * width + x] = static_cast<uint16_t>(((y * width + x) * 256) & 0xFFFF);
    }
  }
  return buffer;
}

} // namespace

// ============================================================================
// Validation: dataOffset out of range
// ============================================================================

TEST(ImageLoaderTest, NegativeDataOffsetReturnsFalse) {
  // A 4x2 RGB8 image = 24 bytes
  auto imageData = makeRawRgb8Buffer(4, 2);
  BufferFileHandler file(imageData.data(), imageData.size());

  DirectImageReference ref(-1, static_cast<uint32_t>(imageData.size()), "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_FALSE(loadImage(file, outFrame, ref, ImageLoadType::Raw));
}

TEST(ImageLoaderTest, DataOffsetBeyondFileSizeReturnsFalse) {
  auto imageData = makeRawRgb8Buffer(4, 2);
  BufferFileHandler file(imageData.data(), imageData.size());

  // dataOffset == fileSize is out of range
  DirectImageReference ref(
      static_cast<int64_t>(imageData.size()),
      static_cast<uint32_t>(imageData.size()),
      "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_FALSE(loadImage(file, outFrame, ref, ImageLoadType::Raw));
}

// ============================================================================
// Validation: dataOffset + dataSize exceeds file size
// ============================================================================

TEST(ImageLoaderTest, DataSizeExceedsFileSizeReturnsFalse) {
  auto imageData = makeRawRgb8Buffer(4, 2);
  BufferFileHandler file(imageData.data(), imageData.size());

  // Start at offset 1, but claim the full file size — goes 1 byte past end
  DirectImageReference ref(1, static_cast<uint32_t>(imageData.size()), "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_FALSE(loadImage(file, outFrame, ref, ImageLoadType::Raw));
}

// ============================================================================
// Validation: compressed offset >= dataSize
// ============================================================================

TEST(ImageLoaderTest, CompressedOffsetExceedsDataSizeReturnsFalse) {
  auto imageData = makeRawRgb8Buffer(4, 2);
  BufferFileHandler file(imageData.data(), imageData.size());

  // compressedOffset (100) >= dataSize (24) should fail
  DirectImageReference ref(
      0,
      static_cast<uint32_t>(imageData.size()),
      "raw/4x2/pixel=rgb8",
      CompressionType::Lz4,
      /*compressedOffset=*/100,
      /*compressedLength=*/10);
  PixelFrame outFrame;
  EXPECT_FALSE(loadImage(file, outFrame, ref, ImageLoadType::Raw));
}

// ============================================================================
// Validation: unsupported compression type (default switch case)
// ============================================================================

TEST(ImageLoaderTest, UnsupportedCompressionTypeReturnsFalse) {
  auto imageData = makeRawRgb8Buffer(4, 2);
  BufferFileHandler file(imageData.data(), imageData.size());

  // Use CompressionType::COUNT which is not handled by any case
  DirectImageReference ref(
      0,
      static_cast<uint32_t>(imageData.size()),
      "raw/4x2/pixel=rgb8",
      CompressionType::COUNT,
      0,
      0);
  PixelFrame outFrame;
  EXPECT_FALSE(loadImage(file, outFrame, ref, ImageLoadType::Raw));
}

// ============================================================================
// Validation: image size mismatch between spec and uncompressedDataSize
// ============================================================================

TEST(ImageLoaderTest, ImageSizeMismatchReturnsFalse) {
  // Create a 4x2 RGB8 image (24 bytes), but claim a different dataSize
  // so that the spec's getRawImageSize() != uncompressedDataSize
  auto imageData = makeRawRgb8Buffer(4, 2);
  // Pad the buffer so the file is large enough for the claimed dataSize
  imageData.resize(100, 0);
  BufferFileHandler file(imageData.data(), imageData.size());

  // The spec "raw/4x2/pixel=rgb8" expects 24 bytes, but we claim 50 bytes
  DirectImageReference ref(0, 50, "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_FALSE(loadImage(file, outFrame, ref, ImageLoadType::Raw));
}

// ============================================================================
// Successful uncompressed raw image load
// ============================================================================

TEST(ImageLoaderTest, LoadUncompressedRawImageSucceeds) {
  const uint32_t width = 4;
  const uint32_t height = 2;
  auto imageData = makeRawRgb8Buffer(width, height);
  BufferFileHandler file(imageData.data(), imageData.size());

  DirectImageReference ref(0, static_cast<uint32_t>(imageData.size()), "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_TRUE(loadImage(file, outFrame, ref, ImageLoadType::Raw));

  // Verify the loaded frame has the correct spec and data
  EXPECT_EQ(outFrame.getWidth(), width);
  EXPECT_EQ(outFrame.getHeight(), height);
  EXPECT_EQ(outFrame.getPixelFormat(), PixelFormat::RGB8);
  EXPECT_EQ(outFrame.getBuffer().size(), imageData.size());
  EXPECT_EQ(outFrame.getBuffer(), imageData);
}

// ============================================================================
// Successful load from memory buffer (void* overload)
// ============================================================================

TEST(ImageLoaderTest, LoadFromMemoryBufferSucceeds) {
  const uint32_t width = 4;
  const uint32_t height = 2;
  auto imageData = makeRawRgb8Buffer(width, height);

  // Use the void* overload — it should ignore dataOffset and set it to 0
  DirectImageReference ref(
      /*dataOffset=*/999, // should be overridden to 0
      static_cast<uint32_t>(imageData.size()),
      "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_TRUE(loadImage(imageData.data(), imageData.size(), outFrame, ref, ImageLoadType::Raw));

  EXPECT_EQ(outFrame.getWidth(), width);
  EXPECT_EQ(outFrame.getHeight(), height);
  EXPECT_EQ(outFrame.getBuffer(), imageData);
}

// ============================================================================
// Load with non-zero dataOffset within valid range
// ============================================================================

TEST(ImageLoaderTest, LoadWithNonZeroDataOffsetSucceeds) {
  const uint32_t width = 4;
  const uint32_t height = 2;
  auto imageData = makeRawRgb8Buffer(width, height);
  const uint32_t imageSize = static_cast<uint32_t>(imageData.size());

  // Prepend some padding bytes before the actual image data
  const size_t padding = 16;
  vector<uint8_t> fileData(padding + imageSize);
  memset(fileData.data(), 0xAB, padding);
  memcpy(fileData.data() + padding, imageData.data(), imageSize);

  BufferFileHandler file(fileData.data(), fileData.size());

  DirectImageReference ref(static_cast<int64_t>(padding), imageSize, "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_TRUE(loadImage(file, outFrame, ref, ImageLoadType::Raw));

  EXPECT_EQ(outFrame.getBuffer(), imageData);
}

// ============================================================================
// Decode path: load a raw image with ImageLoadType::Decode
// ============================================================================

TEST(ImageLoaderTest, LoadRawImageWithDecodeSucceeds) {
  const uint32_t width = 4;
  const uint32_t height = 2;
  auto imageData = makeRawRgb8Buffer(width, height);
  BufferFileHandler file(imageData.data(), imageData.size());

  DirectImageReference ref(0, static_cast<uint32_t>(imageData.size()), "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_TRUE(loadImage(file, outFrame, ref, ImageLoadType::Decode));

  // After decode, the frame should represent the same image with preserved data
  EXPECT_EQ(outFrame.getWidth(), width);
  EXPECT_EQ(outFrame.getHeight(), height);
  EXPECT_EQ(outFrame.getPixelFormat(), PixelFormat::RGB8);
  // Verify pixel data is preserved after decode (RGB8 doesn't need transformation)
  EXPECT_EQ(outFrame.getBuffer(), imageData);
}

// ============================================================================
// Normalize8 path: load a raw image with ImageLoadType::Normalize8
// ============================================================================

TEST(ImageLoaderTest, LoadRawImageWithNormalize8Succeeds) {
  const uint32_t width = 4;
  const uint32_t height = 2;
  auto imageData = makeRawGrey16Buffer(width, height);
  BufferFileHandler file(imageData.data(), imageData.size());

  // Use GREY16 format which will be normalized to GREY8
  DirectImageReference ref(0, static_cast<uint32_t>(imageData.size()), "raw/4x2/pixel=grey16");
  PixelFrame outFrame;
  EXPECT_TRUE(loadImage(file, outFrame, ref, ImageLoadType::Normalize8));

  // After Normalize8, GREY16 should be converted to GREY8
  EXPECT_EQ(outFrame.getWidth(), width);
  EXPECT_EQ(outFrame.getHeight(), height);
  EXPECT_EQ(outFrame.getPixelFormat(), PixelFormat::GREY8);
  // Verify the pixel values are correctly downscaled (shifted right by 8 bits)
  const uint16_t* srcPixels = reinterpret_cast<const uint16_t*>(imageData.data());
  const uint8_t* dstPixels = outFrame.rdata();
  for (uint32_t i = 0; i < width * height; ++i) {
    uint8_t expected = static_cast<uint8_t>(srcPixels[i] >> 8);
    EXPECT_EQ(dstPixels[i], expected) << "Pixel mismatch at index " << i;
  }
}

// ============================================================================
// Normalize16 path: load a raw image with ImageLoadType::Normalize16
// ============================================================================

TEST(ImageLoaderTest, LoadRawImageWithNormalize16Succeeds) {
  const uint32_t width = 4;
  const uint32_t height = 2;
  auto imageData = makeRawGrey16Buffer(width, height);
  BufferFileHandler file(imageData.data(), imageData.size());

  // Use GREY16 format - with Normalize16, it should remain GREY16
  DirectImageReference ref(0, static_cast<uint32_t>(imageData.size()), "raw/4x2/pixel=grey16");
  PixelFrame outFrame;
  EXPECT_TRUE(loadImage(file, outFrame, ref, ImageLoadType::Normalize16));

  // After Normalize16, GREY16 should remain GREY16 (grey16supported=true)
  EXPECT_EQ(outFrame.getWidth(), width);
  EXPECT_EQ(outFrame.getHeight(), height);
  EXPECT_EQ(outFrame.getPixelFormat(), PixelFormat::GREY16);
  // Verify the pixel data is preserved
  EXPECT_EQ(outFrame.getBuffer(), imageData);
}

// ============================================================================
// Load from vector<T> template overload
// ============================================================================

TEST(ImageLoaderTest, LoadFromVectorOverloadSucceeds) {
  const uint32_t width = 4;
  const uint32_t height = 2;
  auto imageData = makeRawRgb8Buffer(width, height);

  DirectImageReference ref(
      /*dataOffset=*/0, static_cast<uint32_t>(imageData.size()), "raw/4x2/pixel=rgb8");
  PixelFrame outFrame;
  EXPECT_TRUE(loadImage(imageData, outFrame, ref, ImageLoadType::Raw));

  EXPECT_EQ(outFrame.getBuffer(), imageData);
}

// ============================================================================
// Image format string that yields unknown size (e.g., "jpg") — size check skipped
// ============================================================================

TEST(ImageLoaderTest, UnknownSizeFormatSkipsSizeCheck) {
  // Create a small buffer that pretends to be a JPEG.
  // The format "jpg" yields kSizeUnknown from getRawImageSize(), so the
  // size mismatch check on line 103 should be skipped.
  // readDiskImageData will read the raw bytes for ImageLoadType::Raw.
  const uint32_t dataSize = 32;
  vector<uint8_t> fakeJpg(dataSize, 0xFF);
  BufferFileHandler file(fakeJpg.data(), fakeJpg.size());

  DirectImageReference ref(0, dataSize, "jpg");
  PixelFrame outFrame;
  // Raw load of a "jpg" format — reads bytes as-is
  EXPECT_TRUE(loadImage(file, outFrame, ref, ImageLoadType::Raw));
  EXPECT_EQ(outFrame.getBuffer().size(), dataSize);
}
