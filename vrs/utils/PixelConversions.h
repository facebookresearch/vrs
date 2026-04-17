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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace vrs::utils::pixel_conversions {

/// Generates a deterministic, low-discrepancy 2D quasi-random sequence of pixel indices using the
/// R2 sequence (plastic constant). Provides uniform spatial coverage without the diagonal artifacts
/// of 1D golden-ratio sampling. Supports range-for iteration.
///
/// Usage:
///   for (uint32_t idx : R2Sampler2D(width, height, maxSamples)) { ... }
class R2Sampler2D {
 public:
  /// @param width   Image width in pixels.
  /// @param height  Image height in pixels.
  /// @param maxSamples  Maximum number of samples (clamped to width * height).
  /// @param stride  Row stride in elements (defaults to width for tightly-packed buffers).
  explicit R2Sampler2D(uint32_t width, uint32_t height, uint32_t maxSamples, uint32_t stride = 0)
      : width_(width),
        height_(height),
        stride_(stride == 0 ? width : stride),
        sampleCount_(std::min(maxSamples, width * height)) {}

  class Iterator {
   public:
    Iterator(uint32_t index, uint32_t width, uint32_t height, uint32_t stride)
        : index_(index), width_(width), height_(height), stride_(stride) {}

    uint32_t operator*() const {
      uint32_t col =
          static_cast<uint32_t>(std::fmod(0.5 + (index_ + 1) * kPlasticInv_, 1.0) * width_);
      uint32_t row =
          static_cast<uint32_t>(std::fmod(0.5 + (index_ + 1) * kPlasticInv2_, 1.0) * height_);
      return row * stride_ + col;
    }

    Iterator& operator++() {
      ++index_;
      return *this;
    }

    bool operator!=(const Iterator& other) const {
      return index_ != other.index_;
    }

   private:
    uint32_t index_;
    uint32_t width_;
    uint32_t height_;
    uint32_t stride_;
  };

  Iterator begin() const {
    return {0, width_, height_, stride_};
  }
  Iterator end() const {
    return {sampleCount_, width_, height_, stride_};
  }
  uint32_t sampleCount() const {
    return sampleCount_;
  }

 private:
  /// Reciprocals of the plastic constant (root of x^3 = x + 1, ~1.3247).
  static constexpr double kPlasticInv_ = 1.0 / 1.3247179572447460;
  static constexpr double kPlasticInv2_ = kPlasticInv_ * kPlasticInv_;

  uint32_t width_;
  uint32_t height_;
  uint32_t stride_;
  uint32_t sampleCount_;
};

/// Convert RGBA8 rows to RGB8 rows, dropping the alpha channel.
/// Optimized with 64-bit wide reads/writes when pointers are 8-byte aligned.
void convertRgbaToRgb(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height);

/// Convert BGR8 pixels to RGB8 pixels by swapping R and B channels.
/// Unrolled to process 8 pixels per iteration for reduced loop overhead.
void convertBgr8ToRgb8(const uint8_t* src, uint8_t* dst, uint32_t pixelCount);

/// Convert RAW10 to GREY8 by extracting the 8 MSB from each 5-byte group.
/// Optimized with 64-bit wide reads/writes when pointers are aligned.
void convertRaw10ToGrey8(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height);

/// Convert single-channel grey pixels to RGB8 by triplicating each grey value.
/// Reads 4 grey bytes via uint32_t, writes 12 RGB bytes via 3x uint32_t.
void convertGreyToRgb8(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height);

/// Clamp an integer to [0, 255].
inline uint8_t clipToUint8(int value) {
  return value < 0 ? 0 : (value > 255 ? 255 : static_cast<uint8_t>(value));
}

/// Convert YUY2 (YUYV) to RGB8.
/// Reads each 4-byte macro-pixel as uint32_t when aligned, writes 2 RGB pixels as bytes.
void convertYuy2ToRgb8(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height);

/// Upscale pixel components from 10/12-bit stored in uint16_t to 16-bit by left-shifting.
void upscalePixels16(const uint16_t* src, uint16_t* dst, uint32_t count, uint16_t bitsToShift);

/// Downscale pixel components from 16/12/10-bit stored in uint16_t to 8-bit by right-shifting.
void downscalePixels16To8(const uint16_t* src, uint8_t* dst, uint32_t count, uint16_t bitsToShift);

/// Normalize float or double buffer to grey8 using p5/p95 percentile range.
/// Samples ~5000 pixels using an R2 quasi-random sequence (plastic constant) for uniform 2D
/// coverage, then maps [p5, p95] to [0, 255] with clamping.
/// NaN, non-finite, and non-positive values are excluded from percentile computation.
/// NaN values are mapped to 0 in the output. Insufficient data produces a blank image.
template <class Float>
void normalizeBuffer(const uint8_t* pixelPtr, uint8_t* outPtr, uint32_t width, uint32_t height);

/// Normalize float buffer to grey8 with a provided [min, max] range.
/// Values outside the range are clamped. NaN values are mapped to 0.
void normalizeBufferWithRange(
    const uint8_t* pixelPtr,
    uint8_t* outPtr,
    uint32_t pixelCount,
    float min,
    float max);

/// Normalize RGB32F or RGBA32F to RGB8 with per-channel dynamic range.
/// NaN values are mapped to 0. Each channel is normalized independently.
/// @param channelCount: 3 for RGB32F, 4 for RGBA32F.
void normalizeRGBXfloatToRGB8(
    const uint8_t* pixelPtr,
    uint8_t* outPtr,
    uint32_t pixelCount,
    size_t channelCount);

} // namespace vrs::utils::pixel_conversions
