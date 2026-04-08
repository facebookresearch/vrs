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

#include <vrs/utils/PixelConversions.h>

#include <vrs/os/CompilerAttributes.h>

#include <cmath>
#include <cstring>
#include <limits>

using namespace std;

namespace vrs::utils::pixel_conversions {

const uint8_t kNaNPixel = 0;

namespace {
template <size_t Alignment>
MAYBE_UNUSED inline bool isAligned(const void* ptr) {
  return (reinterpret_cast<uintptr_t>(ptr) % Alignment) == 0;
}
template <>
MAYBE_UNUSED inline bool isAligned<2>(const void* ptr) {
  return (reinterpret_cast<uintptr_t>(ptr) & 1) == 0;
}
template <>
MAYBE_UNUSED inline bool isAligned<4>(const void* ptr) {
  return (reinterpret_cast<uintptr_t>(ptr) & 3) == 0;
}
template <>
MAYBE_UNUSED inline bool isAligned<8>(const void* ptr) {
  return (reinterpret_cast<uintptr_t>(ptr) & 7) == 0;
}
} // namespace

void convertRgbaToRgb(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height) {
  for (uint32_t h = 0; h < height; ++h) {
    const uint8_t* srcPtr = src + h * srcStride;
    uint8_t* outPtr = dst + h * dstStride;
    uint32_t w = 0;
    // Use wide 64-bit operations only if both line pointers are 8-byte aligned.
    // Within the line, the 8-pixel loop advances src by 32 and dst by 24 bytes,
    // both multiples of 8, so alignment is maintained across iterations.
    if (isAligned<8>(srcPtr) && isAligned<8>(outPtr)) {
      // Process 8 RGBA pixels (32 bytes) -> 8 RGB pixels (24 bytes) per iteration.
      for (; w + 7 < width; w += 8, srcPtr += 32, outPtr += 24) {
        const uint64_t s0 = *reinterpret_cast<const uint64_t*>(srcPtr + 0); // p0,p1
        const uint64_t s1 = *reinterpret_cast<const uint64_t*>(srcPtr + 8); // p2,p3
        const uint64_t s2 = *reinterpret_cast<const uint64_t*>(srcPtr + 16); // p4,p5
        const uint64_t s3 = *reinterpret_cast<const uint64_t*>(srcPtr + 24); // p6,p7
        // Extract 6 RGB bytes from each 64-bit pair (drop alpha bytes at bit 24 and 56).
        const uint64_t rgbs01 = (s0 & 0x0000000000FFFFFFull) | ((s0 >> 8) & 0x0000FFFFFF000000ull);
        const uint64_t rgbs23 = (s1 & 0x0000000000FFFFFFull) | ((s1 >> 8) & 0x0000FFFFFF000000ull);
        const uint64_t rgbs45 = (s2 & 0x0000000000FFFFFFull) | ((s2 >> 8) & 0x0000FFFFFF000000ull);
        const uint64_t rgbs67 = (s3 & 0x0000000000FFFFFFull) | ((s3 >> 8) & 0x0000FFFFFF000000ull);
        // Pack 24 output bytes into 3x64-bit words and store.
        const uint64_t d0 = rgbs01 | ((rgbs23 & 0x000000000000FFFFull) << 48);
        const uint64_t d1 =
            ((rgbs23 >> 16) & 0x00000000FFFFFFFFull) | ((rgbs45 & 0x00000000FFFFFFFFull) << 32);
        const uint64_t d2 = ((rgbs45 >> 32) & 0x000000000000FFFFull) | (rgbs67 << 16);
        *reinterpret_cast<uint64_t*>(outPtr + 0) = d0;
        *reinterpret_cast<uint64_t*>(outPtr + 8) = d1;
        *reinterpret_cast<uint64_t*>(outPtr + 16) = d2;
      }
      // Process 2 pixels at a time with 64-bit read + 6-byte scalar write.
      // srcPtr advances by 8 (still 8-aligned); outPtr writes are byte-level.
      for (; w + 1 < width; w += 2, srcPtr += 8, outPtr += 6) {
        const uint64_t v = *reinterpret_cast<const uint64_t*>(srcPtr);
        outPtr[0] = static_cast<uint8_t>(v >> 0);
        outPtr[1] = static_cast<uint8_t>(v >> 8);
        outPtr[2] = static_cast<uint8_t>(v >> 16);
        outPtr[3] = static_cast<uint8_t>(v >> 32);
        outPtr[4] = static_cast<uint8_t>(v >> 40);
        outPtr[5] = static_cast<uint8_t>(v >> 48);
      }
    }
    // Scalar tail: handles all remaining pixels, or all pixels if unaligned.
    for (; w < width; ++w, srcPtr += 4, outPtr += 3) {
      outPtr[0] = srcPtr[0];
      outPtr[1] = srcPtr[1];
      outPtr[2] = srcPtr[2];
    }
  }
}

void convertBgr8ToRgb8(const uint8_t* src, uint8_t* dst, uint32_t pixelCount) {
  const uint8_t* srcPtr = src;
  uint8_t* outPtr = dst;
  uint32_t i = 0;
  // Process 8 pixels (24 bytes = 3x uint64_t) per iteration when 8-byte aligned.
  // Both src and dst advance by 24 bytes (3x 8), so alignment is maintained.
  if (isAligned<8>(srcPtr) && isAligned<8>(outPtr)) {
    for (; i + 7 < pixelCount; i += 8, srcPtr += 24, outPtr += 24) {
      // Little-endian byte layout across 3 x uint64_t words:
      //   s0: [B0 G0 R0 | B1 G1 R1 | B2 G2]
      //   s1: [R2 | B3 G3 R3 | B4 G4 R4 | B5]
      //   s2: [G5 R5 | B6 G6 R6 | B7 G7 R7]
      // Swap B↔R within each pixel to produce RGB output:
      //   d0: [R0 G0 B0 | R1 G1 B1 | R2 G2]
      //   d1: [B2 | R3 G3 B3 | R4 G4 B4 | R5]
      //   d2: [G5 B5 | R6 G6 B6 | R7 G7 B7]
      const uint64_t s0 = *reinterpret_cast<const uint64_t*>(srcPtr);
      const uint64_t s1 = *reinterpret_cast<const uint64_t*>(srcPtr + 8);
      const uint64_t s2 = *reinterpret_cast<const uint64_t*>(srcPtr + 16);
      // d0: swap bytes 0↔2 (pixel 0), 3↔5 (pixel 1); byte 6 from s1[0]; byte 7 unchanged.
      const uint64_t d0 = ((s0 >> 16) & 0x000000000000FFull) | (s0 & 0x0000000000FF00ull) |
          ((s0 << 16) & 0x00000000FF0000ull) | ((s0 >> 16) & 0x000000FF000000ull) |
          (s0 & 0x0000FF00000000ull) | ((s0 << 16) & 0x00FF0000000000ull) |
          ((s1 << 48) & 0xFF000000000000ull) | (s0 & 0xFF00000000000000ull);
      // d1: byte 0 from s0[6]; swap bytes 1↔3 (pixel 3), 4↔6 (pixel 4); byte 7 from s2[1].
      const uint64_t d1 = ((s0 >> 48) & 0x00000000000000FFull) |
          ((s1 >> 16) & 0x000000000000FF00ull) | (s1 & 0x0000000000FF0000ull) |
          ((s1 << 16) & 0x00000000FF000000ull) | ((s1 >> 16) & 0x000000FF00000000ull) |
          (s1 & 0x0000FF0000000000ull) | ((s1 << 16) & 0x00FF000000000000ull) |
          ((s2 << 48) & 0xFF00000000000000ull);
      // d2: byte 0 unchanged; byte 1 from s1[7]; swap bytes 2↔4 (pixel 6), 5↔7 (pixel 7).
      const uint64_t d2 = (s2 & 0x00000000000000FFull) | ((s1 >> 48) & 0x000000000000FF00ull) |
          ((s2 >> 16) & 0x0000000000FF0000ull) | (s2 & 0x00000000FF000000ull) |
          ((s2 << 16) & 0x000000FF00000000ull) | ((s2 >> 16) & 0x0000FF0000000000ull) |
          (s2 & 0x00FF000000000000ull) | ((s2 << 16) & 0xFF00000000000000ull);
      *reinterpret_cast<uint64_t*>(outPtr) = d0;
      *reinterpret_cast<uint64_t*>(outPtr + 8) = d1;
      *reinterpret_cast<uint64_t*>(outPtr + 16) = d2;
    }
  }
  // Scalar tail: handles remaining pixels or all pixels if unaligned.
  for (; i < pixelCount; ++i, srcPtr += 3, outPtr += 3) {
    outPtr[0] = srcPtr[2];
    outPtr[1] = srcPtr[1];
    outPtr[2] = srcPtr[0];
  }
}

void convertRaw10ToGrey8(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height) {
  const uint8_t* srcPtr = src;
  uint8_t* outPtr = dst;
  for (uint32_t h = 0; h < height; h++, srcPtr += srcStride, outPtr += dstStride) {
    const uint8_t* lineSrcPtr = srcPtr;
    uint8_t* lineOutPtr = outPtr;
    uint32_t group = 0;
    const uint32_t groupCount = width / 4;
    uint64_t d = 0;
    // Process 2 groups (10 src bytes -> 8 dst bytes) at a time with wide writes.
    // The 10-byte source stride cycles through alignments (e.g. 8->2->4->2->8),
    // so we branch on alignment inside the loop to always use the widest read possible.
    // The 8-byte destination stride maintains alignment across iterations.
    while (group + 1 < groupCount && isAligned<8>(lineOutPtr)) {
      if (isAligned<8>(lineSrcPtr)) {
        // 8-byte aligned: 1x uint64_t + 1x uint16_t read
        const uint64_t v = *reinterpret_cast<const uint64_t*>(lineSrcPtr);
        const uint16_t v1 = *reinterpret_cast<const uint16_t*>(lineSrcPtr + 8);
        // v: [M0 M1 M2 M3 L0 M4 M5 M6], v1: [M7 L1]
        d = (v & 0x00000000FFFFFFFFull) | ((v >> 8) & 0x00FFFFFF00000000ull) |
            (static_cast<uint64_t>(v1 & 0xFF) << 56);
      } else if (isAligned<4>(lineSrcPtr)) {
        // 4-byte aligned: 2x uint32_t + 1 byte read
        const uint32_t g0 = *reinterpret_cast<const uint32_t*>(lineSrcPtr); // [M0 M1 M2 M3]
        const uint32_t g1 = *reinterpret_cast<const uint32_t*>(lineSrcPtr + 4); // [L0 M4 M5 M6]
        d = static_cast<uint64_t>(g0) | (static_cast<uint64_t>(g1 >> 8) << 32) |
            (static_cast<uint64_t>(lineSrcPtr[8]) << 56);
      } else if (isAligned<2>(lineSrcPtr)) {
        // 2-byte aligned: 5x uint16_t reads
        const uint16_t w0 = *reinterpret_cast<const uint16_t*>(lineSrcPtr); // [M0 M1]
        const uint16_t w1 = *reinterpret_cast<const uint16_t*>(lineSrcPtr + 2); // [M2 M3]
        const uint16_t w2 = *reinterpret_cast<const uint16_t*>(lineSrcPtr + 4); // [L0 M4]
        const uint16_t w3 = *reinterpret_cast<const uint16_t*>(lineSrcPtr + 6); // [M5 M6]
        const uint16_t w4 = *reinterpret_cast<const uint16_t*>(lineSrcPtr + 8); // [M7 L1]
        d = static_cast<uint64_t>(w0) | (static_cast<uint64_t>(w1) << 16) |
            (static_cast<uint64_t>(w2 >> 8) << 32) | (static_cast<uint64_t>(w3) << 40) |
            (static_cast<uint64_t>(w4 & 0xFF) << 56);
      } else {
        break; // odd alignment, fall to scalar
      }
      *reinterpret_cast<uint64_t*>(lineOutPtr) = d;
      group += 2;
      lineSrcPtr += 10;
      lineOutPtr += 8;
    }
    // Scalar fallback: copy 4 MSB bytes per group, skip LSB byte.
    for (; group < groupCount; group++, lineSrcPtr += 5, lineOutPtr += 4) {
      lineOutPtr[0] = lineSrcPtr[0];
      lineOutPtr[1] = lineSrcPtr[1];
      lineOutPtr[2] = lineSrcPtr[2];
      lineOutPtr[3] = lineSrcPtr[3];
    }
    // Handle remainder pixels (width not a multiple of 4).
    for (uint32_t remainder = 0; remainder < width % 4; remainder++) {
      lineOutPtr[remainder] = lineSrcPtr[remainder];
    }
  }
}

void convertGreyToRgb8(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height) {
  const uint8_t* srcPtr = src;
  uint8_t* outPtr = dst;
  for (uint32_t h = 0; h < height; h++, srcPtr += srcStride, outPtr += dstStride) {
    const uint8_t* lineSrcPtr = srcPtr;
    uint8_t* lineOutPtr = outPtr;
    uint32_t w = 0;
    // Read 4 grey bytes via uint32_t, triplicate each via bit shifts,
    // write 12 RGB bytes as 3x uint32_t stores.
    // Both src (4-byte stride) and dst (12-byte stride) maintain 4-byte alignment.
    if (isAligned<4>(lineSrcPtr) && isAligned<4>(lineOutPtr)) {
      for (; w + 3 < width; w += 4, lineSrcPtr += 4, lineOutPtr += 12) {
        const uint32_t v = *reinterpret_cast<const uint32_t*>(lineSrcPtr);
        const uint32_t g0 = v & 0xFF;
        const uint32_t g1 = (v >> 8) & 0xFF;
        const uint32_t g2 = (v >> 16) & 0xFF;
        const uint32_t g3 = (v >> 24) & 0xFF;
        *reinterpret_cast<uint32_t*>(lineOutPtr) = g0 | (g0 << 8) | (g0 << 16) | (g1 << 24);
        *reinterpret_cast<uint32_t*>(lineOutPtr + 4) = g1 | (g1 << 8) | (g2 << 16) | (g2 << 24);
        *reinterpret_cast<uint32_t*>(lineOutPtr + 8) = g2 | (g3 << 8) | (g3 << 16) | (g3 << 24);
      }
    }
    for (; w < width; w++, lineSrcPtr++, lineOutPtr += 3) {
      lineOutPtr[0] = lineSrcPtr[0];
      lineOutPtr[1] = lineSrcPtr[0];
      lineOutPtr[2] = lineSrcPtr[0];
    }
  }
}

void convertYuy2ToRgb8(
    const uint8_t* src,
    size_t srcStride,
    uint8_t* dst,
    size_t dstStride,
    uint32_t width,
    uint32_t height) {
  const uint8_t* srcPtr = src;
  uint8_t* outPtr = dst;
  for (uint32_t h = 0; h < height; h++, srcPtr += srcStride, outPtr += dstStride) {
    const uint8_t* lineSrcPtr = srcPtr;
    uint8_t* lineOutPtr = outPtr;
    // Source advances by 4 bytes (maintains 4-byte alignment).
    // Output advances by 6 bytes (does NOT maintain 4-byte alignment), so output
    // uses byte writes. The YUV arithmetic dominates the cost anyway.
    const bool srcAligned = isAligned<4>(lineSrcPtr);
    int y0 = 0, u0 = 0, y1 = 0, v0 = 0;
    for (uint32_t w = 0; w < width / 2; w++, lineSrcPtr += 4, lineOutPtr += 6) {
      if (srcAligned) {
        const uint32_t macro = *reinterpret_cast<const uint32_t*>(lineSrcPtr);
        y0 = static_cast<int>(macro & 0xFF);
        u0 = static_cast<int>((macro >> 8) & 0xFF);
        y1 = static_cast<int>((macro >> 16) & 0xFF);
        v0 = static_cast<int>((macro >> 24) & 0xFF);
      } else {
        y0 = lineSrcPtr[0];
        u0 = lineSrcPtr[1];
        y1 = lineSrcPtr[2];
        v0 = lineSrcPtr[3];
      }
      int c = y0 - 16;
      int d = u0 - 128;
      int e = v0 - 128;
      lineOutPtr[0] = clipToUint8((298 * c + 409 * e + 128) >> 8);
      lineOutPtr[1] = clipToUint8((298 * c - 100 * d - 208 * e + 128) >> 8);
      lineOutPtr[2] = clipToUint8((298 * c + 516 * d + 128) >> 8);
      c = y1 - 16;
      lineOutPtr[3] = clipToUint8((298 * c + 409 * e + 128) >> 8);
      lineOutPtr[4] = clipToUint8((298 * c - 100 * d - 208 * e + 128) >> 8);
      lineOutPtr[5] = clipToUint8((298 * c + 516 * d + 128) >> 8);
    }
  }
}

void upscalePixels16(const uint16_t* src, uint16_t* dst, uint32_t count, uint16_t bitsToShift) {
  for (uint32_t i = 0; i < count; ++i) {
    dst[i] = static_cast<uint16_t>(src[i] << bitsToShift);
  }
}

void downscalePixels16To8(const uint16_t* src, uint8_t* dst, uint32_t count, uint16_t bitsToShift) {
  for (uint32_t i = 0; i < count; ++i) {
    dst[i] = static_cast<uint8_t>((src[i] >> bitsToShift) & 0xFF);
  }
}

template <class Float>
void normalizeBuffer(const uint8_t* pixelPtr, uint8_t* outPtr, uint32_t pixelCount) {
  const Float* srcPtr = reinterpret_cast<const Float*>(pixelPtr);
  Float min = 0;
  Float max = 0;
  bool nan = false;
  uint32_t firstPixelIndex = 0;
  while (firstPixelIndex < pixelCount && isnan(srcPtr[firstPixelIndex])) {
    nan = true;
    firstPixelIndex++;
  }
  if (firstPixelIndex < pixelCount) {
    min = max = srcPtr[firstPixelIndex];
    for (uint32_t pixelIndex = firstPixelIndex + 1; pixelIndex < pixelCount; ++pixelIndex) {
      const Float pixel = srcPtr[pixelIndex];
      if (isnan(pixel)) {
        nan = true;
      } else if (pixel < min) {
        min = pixel;
      } else if (pixel > max) {
        max = pixel;
      }
    }
  }
  if (min >= max) {
    // for constant input, blank the image
    memset(outPtr, 0, pixelCount);
  } else {
    const Float factor = numeric_limits<uint8_t>::max() / (max - min);
    if (nan) {
      for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        const Float pixel = srcPtr[pixelIndex];
        outPtr[pixelIndex] =
            isnan(pixel) ? kNaNPixel : static_cast<uint8_t>((pixel - min) * factor);
      }
    } else {
      for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        const Float pixel = srcPtr[pixelIndex];
        outPtr[pixelIndex] = static_cast<uint8_t>((pixel - min) * factor);
      }
    }
  }
}

// Explicit instantiations for float and double.
template void normalizeBuffer<float>(const uint8_t*, uint8_t*, uint32_t);
template void normalizeBuffer<double>(const uint8_t*, uint8_t*, uint32_t);

void normalizeBufferWithRange(
    const uint8_t* pixelPtr,
    uint8_t* outPtr,
    uint32_t pixelCount,
    float min,
    float max) {
  const float* srcPtr = reinterpret_cast<const float*>(pixelPtr);
  const float factor = numeric_limits<uint8_t>::max() / (max - min);
  for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
    const float pixel = srcPtr[pixelIndex];
    outPtr[pixelIndex] = isnan(pixel)
        ? kNaNPixel
        : (pixel <= min       ? 0
               : pixel >= max ? 255
                              : static_cast<uint8_t>((pixel - min) * factor));
  }
}

void normalizeRGBXfloatToRGB8(
    const uint8_t* pixelPtr,
    uint8_t* outPtr,
    uint32_t pixelCount,
    size_t channelCount) {
  const float* srcPtr = reinterpret_cast<const float*>(pixelPtr);
  float channelMin[3] = {0, 0, 0};
  float channelMax[3] = {0, 0, 0};
  bool initialized[3] = {false, false, false};
  bool nan = false;
  // Single pass: find per-channel min/max, track NaN presence.
  for (uint32_t i = 0; i < pixelCount; ++i, srcPtr += channelCount) {
    for (int ch = 0; ch < 3; ++ch) {
      const float v = srcPtr[ch];
      if (isnan(v)) {
        nan = true;
      } else if (!initialized[ch]) {
        channelMin[ch] = channelMax[ch] = v;
        initialized[ch] = true;
      } else if (v < channelMin[ch]) {
        channelMin[ch] = v;
      } else if (v > channelMax[ch]) {
        channelMax[ch] = v;
      }
    }
  }
  float factor[3];
  for (int ch = 0; ch < 3; ++ch) {
    factor[ch] = channelMax[ch] > channelMin[ch]
        ? numeric_limits<uint8_t>::max() / (channelMax[ch] - channelMin[ch])
        : 0;
  }
  srcPtr = reinterpret_cast<const float*>(pixelPtr);
  if (nan) {
    for (uint32_t i = 0; i < pixelCount; ++i, srcPtr += channelCount, outPtr += 3) {
      for (int ch = 0; ch < 3; ++ch) {
        const float v = srcPtr[ch];
        outPtr[ch] = isnan(v) ? kNaNPixel : static_cast<uint8_t>((v - channelMin[ch]) * factor[ch]);
      }
    }
  } else {
    for (uint32_t i = 0; i < pixelCount; ++i, srcPtr += channelCount, outPtr += 3) {
      for (int ch = 0; ch < 3; ++ch) {
        outPtr[ch] = static_cast<uint8_t>((srcPtr[ch] - channelMin[ch]) * factor[ch]);
      }
    }
  }
}

} // namespace vrs::utils::pixel_conversions
