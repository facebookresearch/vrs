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

#include <cstddef>
#include <cstdint>

namespace vrs::utils::pixel_conversions {

/// Convert RGBA8 rows to RGB8 rows, dropping the alpha channel.
void convertRgbaToRgb(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height);

/// Convert BGR8 pixels to RGB8 pixels by swapping R and B channels.
void convertBgr8ToRgb8(const uint8_t* src, uint8_t* dst, uint32_t pixelCount);

/// Convert RAW10 to GREY8 by extracting the 8 MSB from each 5-byte group.
void convertRaw10ToGrey8(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height);

/// Convert single-channel grey pixels to RGB8 by triplicating each grey value.
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

/// Normalize float or double buffer to grey8 with dynamic range calculation.
/// NaN values are mapped to 0. Constant input produces a blank image.
template <class Float>
void normalizeBuffer(const uint8_t* pixelPtr, uint8_t* outPtr, uint32_t pixelCount);

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
