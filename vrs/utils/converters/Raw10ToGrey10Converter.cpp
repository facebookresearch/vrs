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

#include <vrs/utils/converters/Raw10ToGrey10Converter.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#elif defined(__SSE4_2__)
#include <smmintrin.h>
#endif // !__aarch64__ && !__SSE4_2__

#define DEFAULT_LOG_CHANNEL "Raw10ToGrey10Converter"
#include <logging/Checks.h>
#include <logging/Verify.h>

#include <vrs/os/CompilerAttributes.h>

namespace vrs::utils {

// enforce local linkage
namespace {
void convertPixelGroup(const uint8_t* RESTRICT src, uint16_t* RESTRICT dst) {
  const uint16_t r = src[4]; // residual bits
  dst[0] = uint16_t(src[0]) << 2 | ((r & 0x03) >> 0);
  dst[1] = uint16_t(src[1]) << 2 | ((r & 0x0c) >> 2);
  dst[2] = uint16_t(src[2]) << 2 | ((r & 0x30) >> 4);
  dst[3] = uint16_t(src[3]) << 2 | ((r & 0xc0) >> 6);
}

void convertVectorized(
    uint16_t* RESTRICT dst,
    const uint8_t* RESTRICT src,
    const size_t widthInPixels,
    const size_t heightInPixels,
    const size_t strideInBytes,
    bool contiguous) {
#if defined(__aarch64__)

  constexpr uint16x8_t mask = {0x03, 0x0c, 0x30, 0xC0, 0x03, 0x0c, 0x30, 0xC0};
  constexpr int16x8_t rsh = {0, -2, -4, -6, 0, -2, -4, -6};
  constexpr uint8x16_t pshuf = {
      0, 0x80, 1, 0x80, 2, 0x80, 3, 0x80, 5, 0x80, 6, 0x80, 7, 0x80, 8, 0x80};
  constexpr uint8x16_t rshuf = {
      4, 0x80, 4, 0x80, 4, 0x80, 4, 0x80, 9, 0x80, 9, 0x80, 9, 0x80, 9, 0x80};

  constexpr int kSrcIncrement = 10;
  constexpr int kDstIncrement = 8;

  if (contiguous) {
    const int numFullOperations = (heightInPixels * widthInPixels) / 8;
    // The width must be a multiple of 4, so there is at most one leftover pixel group.
    bool hasLeftover = (heightInPixels * widthInPixels) % 8 ? true : false;
    for (int op = 0; op < numFullOperations; ++op) {
      const uint8x16_t encoded = vld1q_u8(src);
      const uint8x16_t r = vqtbl1q_s8(encoded, rshuf);
      const uint8x16_t mIn = vqtbl1q_s8(encoded, pshuf);
      const auto pixels = vshlq_n_u16(mIn, 2);
      const auto fracts = vshlq_u16(vandq_u16(r, mask), rsh);
      vst1q_s16((short*)dst, vorrq_u16(pixels, fracts));

      src += kSrcIncrement;
      dst += kDstIncrement;
    }
    if (hasLeftover) {
      convertPixelGroup(src, dst);
    }
  } else {
    const int numOperationsPerRow = widthInPixels / 8;
    for (int row = 0; row < heightInPixels; ++row) {
      const uint8_t* RESTRICT srcPtr = src;
      for (int op = 0; op < numOperationsPerRow; ++op) {
        const uint8x16_t encoded = vld1q_u8(srcPtr);
        const uint8x16_t r = vqtbl1q_s8(encoded, rshuf);
        const uint8x16_t mIn = vqtbl1q_s8(encoded, pshuf);
        const auto pixels = vshlq_n_u16(mIn, 2);
        const auto fracts = vshlq_u16(vandq_u16(r, mask), rsh);
        vst1q_s16((short*)dst, vorrq_u16(pixels, fracts));

        srcPtr += kSrcIncrement;
        dst += kDstIncrement;
      } // col loop
      src += strideInBytes;
    } // row loop
  } // !contiguous
#elif defined(__SSE4_2__)
  // clang-format off
  static const __m128i mask = _mm_setr_epi8(
      0x03, 0, 0x0c, 0, 0x30, 0, char(0xC0), 0, 0x03, 0, 0x0c, 0, 0x30, 0, char(0xC0), 0);

  static const __m128i pshuf = _mm_setr_epi8(
      0, char(0xFF), 1, char(0xFF), 2, char(0xFF), 3, char(0xFF),
      5, char(0xFF), 6, char(0xFF), 7, char(0xFF), 8, char(0xFF));

  static const __m128i rshuf = _mm_setr_epi8(
      4, char(0xFF), 4, char(0xFF), 4, char(0xFF), 4, char(0xFF),
      9, char(0xFF), 9, char(0xFF), 9, char(0xFF), 9, char(0xFF));

  static const __m128i mult = _mm_setr_epi16(256, 64, 16, 4, 256, 64, 16, 4);

  static const __m128i fshuf = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
  // clang-format on

  constexpr int kSrcIncrement = 10;
  constexpr int kDstIncrement = 8;

  if (contiguous) {
    const int numFullOperations = (heightInPixels * widthInPixels) / 8;
    // The width must be a multiple of 4, so there is at most one leftover pixel group.
    bool hasLeftover = (heightInPixels * widthInPixels) % 8 ? true : false;

    for (int op = 0; op < numFullOperations; ++op) {
      // load 8 pixels plus two fractional
      const __m128i encoded = _mm_lddqu_si128((__m128i*)(src));
      const __m128i r = _mm_shuffle_epi8(encoded, rshuf);
      const __m128i mIn = _mm_shuffle_epi8(encoded, pshuf);
      __m128i pixels = _mm_slli_epi16(mIn, 2);
      __m128i fracts = _mm_and_si128(r, mask);
      fracts = _mm_mullo_epi16(mult, fracts);
      fracts = _mm_shuffle_epi8(fracts, fshuf);
      __m128i idx = _mm_or_si128(pixels, fracts);
      _mm_stream_si128((__m128i*)dst, idx);

      src += kSrcIncrement;
      dst += kDstIncrement;
    }
    if (hasLeftover) {
      convertPixelGroup(src, dst);
    }
  } else {
    const int numOperationsPerRow = widthInPixels / 8;
    for (int row = 0; row < heightInPixels; ++row) {
      const uint8_t* RESTRICT srcPtr = src;

      for (int op = 0; op < numOperationsPerRow; ++op) {
        // load 8 pixels plus two fractional
        const __m128i encoded = _mm_lddqu_si128((__m128i*)(srcPtr));
        const __m128i r = _mm_shuffle_epi8(encoded, rshuf);
        const __m128i mIn = _mm_shuffle_epi8(encoded, pshuf);
        __m128i pixels = _mm_slli_epi16(mIn, 2);
        __m128i fracts = _mm_and_si128(r, mask);
        fracts = _mm_mullo_epi16(mult, fracts);
        fracts = _mm_shuffle_epi8(fracts, fshuf);
        __m128i idx = _mm_or_si128(pixels, fracts);
        _mm_stream_si128((__m128i*)dst, idx);

        srcPtr += kSrcIncrement;
        dst += kDstIncrement;
      } // col loop
      src += strideInBytes;
    } // row loop
  } // !contiguous
#endif // defined(__SSE4_2__)
}

} // namespace

bool convertRaw10ToGrey10(
    void* dstBuffer,
    const void* srcBuffer,
    const size_t widthInPixels,
    const size_t heightInPixels,
    const size_t strideInBytes) {
  // See https://developer.android.com/reference/android/graphics/ImageFormat#RAW10
  // This code is inspired by Bolt's gamma correction library.
  // See that library for documentation of the SIMD intrinsics.

  const uint8_t* RESTRICT src = static_cast<const uint8_t*>(srcBuffer);
  uint16_t* RESTRICT dst = reinterpret_cast<uint16_t*>(dstBuffer);

  const size_t minStride = (widthInPixels * 10) / 8;
  if (!XR_VERIFY(
          (widthInPixels % 4) == 0,
          "RAW10 images must be a multiple of 4 pixels, got width {}",
          widthInPixels)) {
    return false;
  }
  if (!XR_VERIFY(
          strideInBytes >= minStride,
          "RAW10 image stride must be larger or equal than its width. "
          "Got width {} (=min stride {}) and stride {}",
          widthInPixels,
          minStride,
          strideInBytes)) {
    return false;
  }

  const bool contiguous = (strideInBytes == minStride);
  const bool canFullyVectorizeRows = widthInPixels % 8 == 0;

#if defined(__aarch64__) || defined(__SSE4_2__)
  constexpr bool kHasSimdAcceleration = true;
#else
  constexpr bool kHasSimdAcceleration = false;
#endif

  const bool canVectorize = kHasSimdAcceleration && (contiguous || canFullyVectorizeRows);

  if (canVectorize) {
    convertVectorized(dst, src, widthInPixels, heightInPixels, strideInBytes, contiguous);
  } else {
    // Non-SIMD fallback
    constexpr int kSrcIncrement = 5;
    constexpr int kDstIncrement = 4;
    const int numOperationsPerRow = widthInPixels / 4;
    for (int row = 0; row < heightInPixels; ++row) {
      const uint8_t* RESTRICT srcPtr = src;
      for (int op = 0; op < numOperationsPerRow; ++op) {
        convertPixelGroup(srcPtr, dst);
        srcPtr += kSrcIncrement;
        dst += kDstIncrement;
      }
      src += strideInBytes;
    }
  }
  return true;
}

} // namespace vrs::utils
