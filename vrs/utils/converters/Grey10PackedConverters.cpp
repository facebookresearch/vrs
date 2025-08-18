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

#include "Grey10PackedConverters.h"

#include <cstring>

#define DEFAULT_LOG_CHANNEL "Grey10PackedConverters"
#include <logging/Verify.h>

namespace {

#pragma pack(push, 1)
struct PixelGrey10Packed {
  uint16_t p1() const {
    // First 8 bits are in the first byte
    uint16_t low = vals[0] & 0xFF;

    // Next 2 bits are in the second byte
    uint16_t high = vals[1] & 0x3;
    return (high << 8) | low;
  }

  uint16_t p2() const {
    // 6 bits are in the second byte
    uint16_t low = (vals[1] >> 2) & 0x3F;

    // 4 bits are in the third byte
    uint16_t high = vals[2] & 0xF;

    return (high << 6) | low;
  }

  uint16_t p3() const {
    // 4 bits are in the third byte
    uint16_t low = (vals[2] >> 4) & 0xF;

    // 6 bits are in the fourth byte
    uint16_t high = vals[3] & 0x3F;

    return (high << 4) | low;
  }

  uint16_t p4() const {
    // 2 bits are in the fourth byte
    uint16_t low = (vals[3] >> 6) & 0x3;

    // 8 bits are in the fifth byte
    uint16_t high = vals[4] & 0xFF;

    return (high << 2) | low;
  }

  void set_p1(uint16_t val) {
    // First 8 bits are in the first byte
    vals[0] = val & 0xFF;

    // Next 2 bits are in the second byte
    // First clear the bits, then set them
    vals[1] &= ~(0x3);
    vals[1] |= (val >> 8) & 0x3;
  }

  void set_p2(uint16_t val) {
    // 6 bits are in the second byte
    // First clear the bits, then set them
    vals[1] &= ~(0x3F << 2);
    vals[1] |= (val & 0x3F) << 2;

    // 4 bits are in the third byte
    // First clear the bits, then set them
    vals[2] &= ~(0xF);
    vals[2] |= (val >> 6) & 0xF;
  }

  void set_p3(uint16_t val) {
    // 4 bits are in the third byte
    // First clear the bits, then set them
    vals[2] &= ~(0xF << 4);
    vals[2] |= (val & 0xF) << 4;

    // 6 bits are in the fourth byte
    // First clear the bits, then set them
    vals[3] &= ~(0x3F);
    vals[3] |= (val >> 4) & 0x3F;
  }

  void set_p4(uint16_t val) {
    // 2 bits are in the fourth byte
    // First clear the bits, then set them
    vals[3] &= ~(0x3 << 6);
    vals[3] |= (val & 0x3) << 6;

    // 8 bits are in the fifth byte
    vals[4] = (val >> 2) & 0xFF;
  }

 private:
  uint8_t vals[5];
};
#pragma pack(pop)

} // namespace

namespace vrs::utils {

bool convertGrey10PackedToGrey16(
    void* dst,
    uint32_t dstSize,
    uint32_t dstStride,
    const void* src,
    uint32_t srcSize,
    uint32_t width,
    uint32_t height,
    uint32_t srcStride) {
  if (!XR_VERIFY(src != nullptr && dst != nullptr && width > 0 && height > 0)) {
    return false;
  }

  if (!XR_VERIFY(
          srcSize >= srcStride * height,
          "Source buffer too small. Got size {} but need {}",
          srcSize,
          srcStride * height)) {
    return false;
  }

  if (!XR_VERIFY(
          srcStride >= (width + 3) / 4 * 5,
          "Source stride too small. Got {} but need {}",
          srcStride,
          width * 10 / 8)) {
    return false;
  }

  if (!XR_VERIFY(
          dstSize >= dstStride * height,
          "Destination buffer too small. Got size {} but need {}",
          dstSize,
          dstStride * height)) {
    return false;
  }

  const uint8_t* input = static_cast<const uint8_t*>(src);
  uint8_t* output = static_cast<uint8_t*>(dst);

  for (uint32_t i = 0; i < height; i++) {
    const struct PixelGrey10Packed* grey10packed =
        reinterpret_cast<const struct PixelGrey10Packed*>(input + srcStride * i);
    uint16_t* grey10 = reinterpret_cast<uint16_t*>(output + dstStride * i);

    const uint32_t total_quadruplets_in_row = width / 4;
    for (uint32_t j = 0; j < total_quadruplets_in_row; j++) {
      grey10[0] = grey10packed->p1() << 6;
      grey10[1] = grey10packed->p2() << 6;
      grey10[2] = grey10packed->p3() << 6;
      grey10[3] = grey10packed->p4() << 6;
      grey10packed++;
      grey10 += 4;
    }
    // copy leftover pixels, if any
    uint32_t w = total_quadruplets_in_row * 4;
    if (w < width) {
      grey10[0] = grey10packed->p1();
      if (++w < width) {
        grey10[1] = grey10packed->p2();
        if (++w < width) {
          grey10[2] = grey10packed->p3();
        }
      }
    }
  }

  return true;
}

bool convertGrey10PackedToGrey8(
    void* dst,
    uint32_t dstSize,
    uint32_t dstStride,
    const void* src,
    uint32_t srcSize,
    uint32_t width,
    uint32_t height,
    uint32_t srcStride) {
  if (!XR_VERIFY(src != nullptr && dst != nullptr && width > 0 && height > 0)) {
    return false;
  }

  if (!XR_VERIFY(
          srcSize >= srcStride * height,
          "Source buffer too small. Got size {} but need {}",
          srcSize,
          srcStride * height)) {
    return false;
  }

  if (!XR_VERIFY(
          srcStride >= (width + 3) / 4 * 5,
          "Source stride too small. Got {} but need {}",
          srcStride,
          width * 10 / 8)) {
    return false;
  }

  if (!XR_VERIFY(
          dstSize >= dstStride * height,
          "Destination buffer too small. Got size {} but need {}",
          dstSize,
          dstStride * height)) {
    return false;
  }

  const uint8_t* input = static_cast<const uint8_t*>(src);
  uint8_t* output = static_cast<uint8_t*>(dst);

  for (uint32_t i = 0; i < height; i++) {
    const struct PixelGrey10Packed* grey10packed =
        reinterpret_cast<const struct PixelGrey10Packed*>(input + srcStride * i);
    uint8_t* grey8 = reinterpret_cast<uint8_t*>(output + dstStride * i);

    const uint32_t total_quadruplets_in_row = width / 4;
    for (uint32_t j = 0; j < total_quadruplets_in_row; j++) {
      grey8[0] = static_cast<uint8_t>(grey10packed->p1() >> 2);
      grey8[1] = static_cast<uint8_t>(grey10packed->p2() >> 2);
      grey8[2] = static_cast<uint8_t>(grey10packed->p3() >> 2);
      grey8[3] = static_cast<uint8_t>(grey10packed->p4() >> 2);
      grey10packed++;
      grey8 += 4;
    }
    // copy leftover pixels, if any
    uint32_t w = total_quadruplets_in_row * 4;
    if (w < width) {
      grey8[0] = static_cast<uint8_t>(grey10packed->p1() >> 2);
      if (++w < width) {
        grey8[1] = static_cast<uint8_t>(grey10packed->p2() >> 2);
        if (++w < width) {
          grey8[2] = static_cast<uint8_t>(grey10packed->p3() >> 2);
        }
      }
    }
  }

  return true;
}

bool convertGrey10ToGrey10Packed(
    void* dst,
    uint32_t dstSize,
    uint32_t dstStride,
    const void* src,
    uint32_t srcSize,
    uint32_t width,
    uint32_t height,
    uint32_t srcStride) {
  if (!XR_VERIFY(src != nullptr && dst != nullptr && width > 0 && height > 0)) {
    return false;
  }

  if (!XR_VERIFY(
          srcSize >= srcStride * height,
          "Source buffer too small. Got size {} but need {}",
          srcSize,
          srcStride * height)) {
    return false;
  }

  if (!XR_VERIFY(
          srcStride >= width * 2,
          "Source stride too small. Got {} but need {}",
          srcStride,
          width * 2)) {
    return false;
  }

  if (!XR_VERIFY(
          dstSize >= dstStride * height,
          "Destination buffer too small. Got size {} but need {}",
          dstSize,
          dstStride * height)) {
    return false;
  }

  if (!XR_VERIFY(
          dstStride >= (width + 3) / 4 * 5,
          "Destination stride too small. Got {} but need {}",
          dstStride,
          (width + 3) / 4 * 5)) {
    return false;
  }

  const uint8_t* input = static_cast<const uint8_t*>(src);
  uint8_t* output = static_cast<uint8_t*>(dst);

  for (uint32_t i = 0; i < height; i++) {
    const uint16_t* grey10 = reinterpret_cast<const uint16_t*>(input + srcStride * i);
    struct PixelGrey10Packed* grey10packed =
        reinterpret_cast<struct PixelGrey10Packed*>(output + dstStride * i);

    const uint32_t total_quadruplets_in_row = width / 4;
    for (uint32_t j = 0; j < total_quadruplets_in_row; j++) {
      grey10packed->set_p1(grey10[0]);
      grey10packed->set_p2(grey10[1]);
      grey10packed->set_p3(grey10[2]);
      grey10packed->set_p4(grey10[3]);
      grey10packed++;
      grey10 += 4;
    }

    // Handle leftover pixels, if any
    uint32_t w = total_quadruplets_in_row * 4;
    if (w < width) {
      memset(grey10packed, 0, sizeof(struct PixelGrey10Packed));
      grey10packed->set_p1(grey10[0]);
      if (++w < width) {
        grey10packed->set_p2(grey10[1]);
        if (++w < width) {
          grey10packed->set_p3(grey10[2]);
        }
      }
    }
  }

  return true;
}

} // namespace vrs::utils
