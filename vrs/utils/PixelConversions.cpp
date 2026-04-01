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

#include <cmath>
#include <cstring>
#include <limits>

using namespace std;

namespace vrs::utils::pixel_conversions {

const uint8_t kNaNPixel = 0;

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
    for (uint32_t w = 0; w < width; ++w) {
      outPtr[0] = srcPtr[0];
      outPtr[1] = srcPtr[1];
      outPtr[2] = srcPtr[2];
      outPtr += 3;
      srcPtr += 4;
    }
  }
}

void convertBgr8ToRgb8(const uint8_t* src, uint8_t* dst, uint32_t pixelCount) {
  const uint8_t* srcPtr = src;
  uint8_t* outPtr = dst;
  for (uint32_t i = 0; i < pixelCount; ++i) {
    outPtr[0] = srcPtr[2];
    outPtr[1] = srcPtr[1];
    outPtr[2] = srcPtr[0];
    srcPtr += 3;
    outPtr += 3;
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
    for (uint32_t group = 0; group < width / 4; group++, lineSrcPtr += 5, lineOutPtr += 4) {
      lineOutPtr[0] = lineSrcPtr[0];
      lineOutPtr[1] = lineSrcPtr[1];
      lineOutPtr[2] = lineSrcPtr[2];
      lineOutPtr[3] = lineSrcPtr[3];
    }
    // width is most probably a multiple of 4. In case it isn't...
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
    for (uint32_t w = 0; w < width; w++, lineSrcPtr++, lineOutPtr += 3) {
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
    for (uint32_t w = 0; w < width / 2; w++, lineSrcPtr += 4, lineOutPtr += 6) {
      int y0 = lineSrcPtr[0];
      int u0 = lineSrcPtr[1];
      int y1 = lineSrcPtr[2];
      int v0 = lineSrcPtr[3];
      int c = y0 - 16;
      int d = u0 - 128;
      int e = v0 - 128;
      lineOutPtr[2] = clipToUint8((298 * c + 516 * d + 128) >> 8); // blue
      lineOutPtr[1] = clipToUint8((298 * c - 100 * d - 208 * e + 128) >> 8); // green
      lineOutPtr[0] = clipToUint8((298 * c + 409 * e + 128) >> 8); // red
      c = y1 - 16;
      lineOutPtr[5] = clipToUint8((298 * c + 516 * d + 128) >> 8); // blue
      lineOutPtr[4] = clipToUint8((298 * c - 100 * d - 208 * e + 128) >> 8); // green
      lineOutPtr[3] = clipToUint8((298 * c + 409 * e + 128) >> 8); // red
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

struct Triplet {
  uint8_t r, g, b;
};
struct TripletF {
  float r, g, b;
};

void normalizeRGBXfloatToRGB8(
    const uint8_t* pixelPtr,
    uint8_t* outPtr,
    uint32_t pixelCount,
    size_t channelCount) {
  const float* srcPtr = reinterpret_cast<const float*>(pixelPtr);
  TripletF min{0, 0, 0};
  TripletF max{0, 0, 0};
  // initialize min & max for each dimension.
  Triplet init{0, 0, 0};
  for (uint32_t pixelIndex = 0; pixelIndex < pixelCount && (init.r + init.g + init.b) < 3;
       ++pixelIndex, srcPtr += channelCount) {
    const TripletF* pixel = reinterpret_cast<const TripletF*>(srcPtr);
    if (init.r == 0 && !isnan(pixel->r)) {
      min.r = max.r = pixel->r;
      init.r = 1;
    }
    if (init.g == 0 && !isnan(pixel->g)) {
      min.g = max.g = pixel->g;
      init.g = 1;
    }
    if (init.b == 0 && !isnan(pixel->b)) {
      min.b = max.b = pixel->b;
      init.b = 1;
    }
  }
  bool nan = false;
  srcPtr = reinterpret_cast<const float*>(pixelPtr);
  for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex, srcPtr += channelCount) {
    const TripletF* pixel = reinterpret_cast<const TripletF*>(srcPtr);
    if (isnan(pixel->r)) {
      nan = true;
    } else if (pixel->r < min.r) {
      min.r = pixel->r;
    } else if (pixel->r > max.r) {
      max.r = pixel->r;
    }
    if (isnan(pixel->g)) {
      nan = true;
    } else if (pixel->g < min.g) {
      min.g = pixel->g;
    } else if (pixel->g > max.g) {
      max.g = pixel->g;
    }
    if (isnan(pixel->b)) {
      nan = true;
    } else if (pixel->b < min.b) {
      min.b = pixel->b;
    } else if (pixel->b > max.b) {
      max.b = pixel->b;
    }
  }
  const float factorR = max.r > min.r ? numeric_limits<uint8_t>::max() / (max.r - min.r) : 0;
  const float factorG = max.g > min.g ? numeric_limits<uint8_t>::max() / (max.g - min.g) : 0;
  const float factorB = max.b > min.b ? numeric_limits<uint8_t>::max() / (max.b - min.b) : 0;
  srcPtr = reinterpret_cast<const float*>(pixelPtr);
  if (nan) {
    for (uint32_t pixelIndex = 0; pixelIndex < pixelCount;
         ++pixelIndex, srcPtr += channelCount, outPtr += 3) {
      const TripletF* pixel = reinterpret_cast<const TripletF*>(srcPtr);
      Triplet* out = reinterpret_cast<Triplet*>(outPtr);
      out->r = isnan(pixel->r) ? kNaNPixel : static_cast<uint8_t>((pixel->r - min.r) * factorR);
      out->g = isnan(pixel->g) ? kNaNPixel : static_cast<uint8_t>((pixel->g - min.g) * factorG);
      out->b = isnan(pixel->b) ? kNaNPixel : static_cast<uint8_t>((pixel->b - min.b) * factorB);
    }
  } else {
    for (uint32_t pixelIndex = 0; pixelIndex < pixelCount;
         ++pixelIndex, srcPtr += channelCount, outPtr += 3) {
      const TripletF* pixel = reinterpret_cast<const TripletF*>(srcPtr);
      Triplet* out = reinterpret_cast<Triplet*>(outPtr);
      out->r = static_cast<uint8_t>((pixel->r - min.r) * factorR);
      out->g = static_cast<uint8_t>((pixel->g - min.g) * factorG);
      out->b = static_cast<uint8_t>((pixel->b - min.b) * factorB);
    }
  }
}

} // namespace vrs::utils::pixel_conversions
