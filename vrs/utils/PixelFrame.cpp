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

#include "PixelFrame.h"

#include <atomic>
#include <cstring>

#define DEFAULT_LOG_CHANNEL "PixelFrame"
#include <logging/Checks.h>
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/helpers/FileMacros.h>
#include <vrs/utils/converters/Raw10ToGrey10Converter.h>

using namespace std;

namespace {

static const uint8_t kNaNPixel = 0;

/// normalize floating point pixels to grey8
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

} // namespace

namespace vrs::utils {

PixelFrame::PixelFrame(const ImageContentBlockSpec& spec)
    : imageSpec_{
          spec.getPixelFormat(),
          spec.getWidth(),
          spec.getHeight(),
          spec.getRawStride(),
          spec.getRawStride2()} {
  size_t size = imageSpec_.getRawImageSize();
  if (XR_VERIFY(size != ContentBlock::kSizeUnknown)) {
    frameBytes_.resize(size);
  }
}

void PixelFrame::init(const ImageContentBlockSpec& spec) {
  if (!hasSamePixels(spec)) {
    imageSpec_ = spec;
    size_t size = imageSpec_.getRawImageSize();
    if (XR_VERIFY(size != ContentBlock::kSizeUnknown)) {
      frameBytes_.resize(size);
    }
  }
}

void PixelFrame::init(shared_ptr<PixelFrame>& inOutFrame, const ImageContentBlockSpec& spec) {
  if (!inOutFrame) {
    inOutFrame = make_shared<PixelFrame>(spec);
  } else {
    inOutFrame->init(spec);
  }
}

void PixelFrame::swap(PixelFrame& other) {
  if (!hasSamePixels(other.imageSpec_)) {
    ImageContentBlockSpec tempSpec = other.imageSpec_;
    other.imageSpec_ = imageSpec_;
    imageSpec_ = tempSpec;
  }
  frameBytes_.swap(other.frameBytes_);
}

bool PixelFrame::hasSamePixels(const ImageContentBlockSpec& spec) const {
  return getPixelFormat() == spec.getPixelFormat() && getWidth() == spec.getWidth() &&
      getHeight() == spec.getHeight() && getStride() == spec.getStride();
}

void PixelFrame::blankFrame() {
  if (getPixelFormat() != PixelFormat::RGBA8) {
    memset(wdata(), 0, frameBytes_.size());
  } else {
    uint32_t solidBlack = 0xff000000;
    uint32_t stride = getStride();
    uint32_t width = getWidth();
    uint8_t* pixels = wdata();
    for (uint32_t h = 0; h < getHeight(); ++h) {
      uint32_t* line = reinterpret_cast<uint32_t*>(pixels + h * stride);
      for (uint32_t w = 0; w < width; ++w) {
        line[w] = solidBlack;
      }
    }
  }
}

bool PixelFrame::readFrame(
    shared_ptr<PixelFrame>& frame,
    RecordReader* reader,
    const ContentBlock& cb) {
  if (!XR_VERIFY(cb.getContentType() == ContentType::IMAGE)) {
    return false;
  }
  if (cb.image().getImageFormat() == ImageFormat::VIDEO) {
    return false;
  }
  if (!frame) {
    frame = make_shared<PixelFrame>();
  }
  switch (cb.image().getImageFormat()) {
    case ImageFormat::RAW:
      return frame->readRawFrame(reader, cb.image());
    case ImageFormat::PNG:
      return frame->readPngFrame(reader, cb.getBlockSize());
    case ImageFormat::JPG:
      return frame->readJpegFrame(reader, cb.getBlockSize());
    case ImageFormat::JXL:
      return frame->readJxlFrame(reader, cb.getBlockSize());
    default:
      return false;
  }
  return false;
}

bool PixelFrame::readRawFrame(RecordReader* reader, const ImageContentBlockSpec& inputImageSpec) {
  // Read multiplane images as is
  if (inputImageSpec.getPlaneCount() != 1) {
    init(inputImageSpec);
    return VERIFY_SUCCESS(reader->read(wdata(), size()));
  }
  // remove stride of single plane raw images
  ImageContentBlockSpec noStrideSpec{
      inputImageSpec.getPixelFormat(), inputImageSpec.getWidth(), inputImageSpec.getHeight()};
  if (inputImageSpec.getStride() == noStrideSpec.getStride()) {
    init(inputImageSpec);
    return VERIFY_SUCCESS(reader->read(wdata(), size()));
  }
  init(noStrideSpec);
  // remove stride or extra bytes
  const size_t contentStride = inputImageSpec.getStride();
  const size_t frameStride = imageSpec_.getStride();
  vector<char> strideGap;
  if (contentStride > frameStride) {
    strideGap.resize(contentStride - frameStride);
  }
  uint8_t* wdata = frameBytes_.data();
  for (uint32_t line = 0; line < this->getHeight(); line++) {
    IF_ERROR_LOG_AND_RETURN_FALSE(reader->read(wdata, frameStride));
    if (!strideGap.empty()) {
      int readStrideStatus =
          strideGap.size() <= reader->getUnreadBytes() ? reader->read(strideGap) : NOT_ENOUGH_DATA;
      if (readStrideStatus != 0) {
        if (line < this->getHeight() - 1) {
          IF_ERROR_LOG_AND_RETURN_FALSE(readStrideStatus);
        }
        // Compromise for data collected without expected stride for the last line only
        // Please don't remove/reduce the warnings, fix the recording app instead!
        // We do want to be loud, but for a while only.
        static atomic<int> sSpamLimiter(500);
        if (sSpamLimiter.fetch_add(-1, memory_order_relaxed) > 0) {
          XR_LOGW("Stride data missing for the last line. Please fix the recording app.");
        } else {
          XR_LOGW_EVERY_N_SEC(5, "Stride data still missing. Silencing warnings for 5 seconds...");
        }
      }
    }
    wdata += frameStride;
  }
  return true;
}

bool PixelFrame::readCompressedFrame(const std::vector<uint8_t>& pixels, ImageFormat imageFormat) {
  switch (imageFormat) {
    case vrs::ImageFormat::JPG:
      return readJpegFrame(pixels);
    case vrs::ImageFormat::JXL:
      return readJxlFrame(pixels);
    case vrs::ImageFormat::PNG:
      return readPngFrame(pixels);
    default:
      return false;
  }
  return false;
}

bool PixelFrame::readRawFrame(
    shared_ptr<PixelFrame>& frame,
    RecordReader* reader,
    const ImageContentBlockSpec& inputImageSpec) {
  if (!frame) {
    frame = make_shared<PixelFrame>(inputImageSpec);
  }
  return frame->readRawFrame(reader, inputImageSpec);
}

void PixelFrame::normalizeFrame(
    const shared_ptr<PixelFrame>& sourceFrame,
    shared_ptr<PixelFrame>& outFrame,
    bool grey16supported) {
  if (!sourceFrame->normalizeFrame(outFrame, grey16supported)) {
    outFrame = sourceFrame;
  }
}

PixelFormat PixelFrame::getNormalizedPixelFormat(
    PixelFormat sourcePixelFormat,
    bool grey16supported) {
  return ImageContentBlockSpec::getChannelCountPerPixel(sourcePixelFormat) > 1 ? PixelFormat::RGB8
      : grey16supported                                                        ? PixelFormat::GREY16
                                                                               : PixelFormat::GREY8;
}

bool PixelFrame::inplaceRgbaToRgb() {
  if (getPixelFormat() != PixelFormat::RGBA8) {
    return false;
  }
  uint32_t width = getWidth();
  uint32_t height = getHeight();
  ImageContentBlockSpec rgbSpec(PixelFormat::RGB8, width, height);
  size_t stride = getStride();
  size_t rgbStride = rgbSpec.getStride();
  for (uint32_t h = 0; h < height; ++h) {
    const uint8_t* srcPtr = rdata() + h * stride;
    uint8_t* outPtr = wdata() + h * rgbStride;
    for (uint32_t w = 0; w < width; ++w) {
      outPtr[0] = srcPtr[0];
      outPtr[1] = srcPtr[1];
      outPtr[2] = srcPtr[2];
      outPtr += 3;
      srcPtr += 4;
    }
  }
  imageSpec_ = rgbSpec;
  frameBytes_.resize(imageSpec_.getHeight() * imageSpec_.getStride());
  return true;
}

bool PixelFrame::convertRgbaToRgb(std::shared_ptr<PixelFrame>& outRgbFrame) const {
  if (getPixelFormat() != PixelFormat::RGBA8) {
    return false;
  }
  uint32_t width = getWidth();
  uint32_t height = getHeight();
  init(outRgbFrame, ImageContentBlockSpec(PixelFormat::RGB8, width, height));
  size_t srcStride = getStride();
  size_t outStride = outRgbFrame->getStride();
  for (uint32_t h = 0; h < height; ++h) {
    const uint8_t* srcPtr = rdata() + h * srcStride;
    uint8_t* outPtr = outRgbFrame->wdata() + h * outStride;
    for (uint32_t w = 0; w < width; ++w) {
      outPtr[0] = srcPtr[0];
      outPtr[1] = srcPtr[1];
      outPtr[2] = srcPtr[2];
      outPtr += 3;
      srcPtr += 4;
    }
  }
  return true;
}

inline uint8_t clipToUint8(int value) {
  return value < 0 ? 0 : (value > 255 ? 255 : static_cast<uint8_t>(value));
}

bool PixelFrame::normalizeFrame(shared_ptr<PixelFrame>& normalizedFrame, bool grey16supported)
    const {
  uint16_t bitsToShift = 0;
  uint32_t componentCount = 0;
  PixelFormat srcFormat = imageSpec_.getPixelFormat();
  // See if we can convert to something simple enough using Ocean
  PixelFormat targetPixelFormat = getNormalizedPixelFormat(srcFormat, grey16supported);
  if (srcFormat == targetPixelFormat) {
    return false;
  }
  if (normalizedFrame.get() == this) {
    normalizedFrame.reset();
  }
  if (normalizeToPixelFormat(normalizedFrame, targetPixelFormat)) {
    return true;
  }
  PixelFormat format = srcFormat;
  switch (srcFormat) {
    case PixelFormat::YUV_I420_SPLIT:
    case PixelFormat::YUV_420_NV21:
    case PixelFormat::YUV_420_NV12: {
      // buffer truncation to grayscale when ocean isn't available
      const uint32_t width = imageSpec_.getWidth();
      const uint32_t height = imageSpec_.getHeight();
      const uint32_t stride = imageSpec_.getStride();
      init(normalizedFrame, PixelFormat::GREY8, width, height);
      const uint8_t* src = rdata();
      uint8_t* dst = normalizedFrame->wdata();
      for (uint32_t line = 0; line < height; line++) {
        memcpy(dst, src, width);
        src += stride;
        dst += width;
      }
      return true;
    }
    case PixelFormat::GREY10:
      if (grey16supported) {
        format = PixelFormat::GREY16;
        bitsToShift = 6;
        componentCount = 1;
      } else {
        format = PixelFormat::GREY8;
        bitsToShift = 2;
        componentCount = 1;
      }
      break;
    case PixelFormat::GREY12:
      if (grey16supported) {
        format = PixelFormat::GREY16;
        bitsToShift = 4;
        componentCount = 1;
      } else {
        format = PixelFormat::GREY8;
        bitsToShift = 4;
        componentCount = 1;
      }
      break;
    case PixelFormat::GREY16:
      if (grey16supported) {
        // nothing to do!
      } else {
        format = PixelFormat::GREY8;
        bitsToShift = 8;
        componentCount = 1;
      }
      break;
    case PixelFormat::RGB10:
      format = PixelFormat::RGB8;
      bitsToShift = 2;
      componentCount = 3;
      break;
    case PixelFormat::RGB12:
      format = PixelFormat::RGB8;
      bitsToShift = 4;
      componentCount = 3;
      break;
    case PixelFormat::BGR8:
      format = PixelFormat::RGB8;
      componentCount = 3;
      break;
    case PixelFormat::RGB32F:
      format = PixelFormat::RGB8;
      componentCount = 3;
      break;
    case PixelFormat::RGBA32F:
      format = PixelFormat::RGB8;
      componentCount = 3;
      break;
    case PixelFormat::DEPTH32F:
    case PixelFormat::SCALAR64F:
    case PixelFormat::BAYER8_RGGB:
      format = PixelFormat::GREY8;
      componentCount = 1;
      break;
    case PixelFormat::RGB_IR_RAW_4X4:
      format = PixelFormat::RGB8;
      componentCount = 1;
      break;
    case PixelFormat::RAW10:
    case PixelFormat::RAW10_BAYER_RGGB:
    case PixelFormat::RAW10_BAYER_BGGR:
      if (grey16supported) {
        format = PixelFormat::GREY16;
        bitsToShift = 6;
        componentCount = 1;
      } else {
        format = PixelFormat::GREY8;
        bitsToShift = 2;
        componentCount = 1;
      }
      break;
    case PixelFormat::YUY2:
      format = PixelFormat::RGB8;
      componentCount = 3;
      break;
    default:
      break;
  }
  if (format == srcFormat) {
    return false; // no conversion needed or supported
  }
  init(normalizedFrame, format, getWidth(), getHeight());
  if (srcFormat == PixelFormat::BGR8) {
    // swap R & B
    const uint8_t* srcPtr = rdata();
    uint8_t* outPtr = normalizedFrame->wdata();
    const uint32_t pixelCount = getWidth() * getHeight();
    for (uint32_t i = 0; i < pixelCount; ++i) {
      outPtr[2] = srcPtr[0];
      outPtr[1] = srcPtr[1];
      outPtr[0] = srcPtr[2];
      srcPtr += 3;
      outPtr += 3;
    }
  } else if (srcFormat == PixelFormat::RGB32F) {
    // normalize float pixels to rgb8
    normalizeRGBXfloatToRGB8(rdata(), normalizedFrame->wdata(), getWidth() * getHeight(), 3);
  } else if (srcFormat == PixelFormat::RGBA32F) {
    // normalize float pixels to rgb8, drop alpha channel
    normalizeRGBXfloatToRGB8(rdata(), normalizedFrame->wdata(), getWidth() * getHeight(), 4);
  } else if (srcFormat == PixelFormat::DEPTH32F) {
    // normalize float pixels to grey8
    normalizeBuffer<float>(rdata(), normalizedFrame->wdata(), getWidth() * getHeight());
  } else if (srcFormat == PixelFormat::SCALAR64F) {
    // normalize double pixels to grey8
    normalizeBuffer<double>(rdata(), normalizedFrame->wdata(), getWidth() * getHeight());
  } else if (srcFormat == PixelFormat::BAYER8_RGGB) {
    // display as grey8(copy) for now
    const uint8_t* srcPtr = rdata();
    uint8_t* outPtr = normalizedFrame->wdata();
    const uint32_t pixelCount = getWidth() * getHeight() * componentCount;
    for (uint32_t i = 0; i < pixelCount; ++i) {
      outPtr[i] = srcPtr[i];
    }
  } else if (
      srcFormat == PixelFormat::RAW10 || srcFormat == PixelFormat::RAW10_BAYER_RGGB ||
      srcFormat == PixelFormat::RAW10_BAYER_BGGR) {
    if (format == PixelFormat::GREY16) {
      // Convert from RAW10 to GREY10 directly into the output buffer
      if (!convertRaw10ToGrey10(
              normalizedFrame->wdata(), rdata(), getWidth(), getHeight(), getStride())) {
        return false;
      }
      uint16_t* ptr = reinterpret_cast<uint16_t*>(normalizedFrame->wdata());
      const uint32_t pixelCount = getWidth() * getHeight() * componentCount;
      for (uint32_t i = 0; i < pixelCount; ++i) {
        ptr[i] <<= bitsToShift;
      }
    } else {
      // source: 4 bytes with 8 msb data, 1 byte with 4x 2 lsb of data
      // convert to GREY8 by copying 4 bytes of msb data, and dropping the 5th of lsb data...
      const uint8_t* srcPtr = rdata();
      const size_t srcStride = getStride();
      uint8_t* outPtr = normalizedFrame->wdata();
      const size_t outStride = normalizedFrame->getStride();
      const uint32_t width = getWidth();
      for (uint32_t h = 0; h < getHeight(); h++, srcPtr += srcStride, outPtr += outStride) {
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
  } else if (srcFormat == PixelFormat::RGB_IR_RAW_4X4) {
    // This is a placeholder implementation that simply writes out the source data in R, G and B.
    const uint8_t* srcPtr = rdata();
    const size_t srcStride = getStride();
    uint8_t* outPtr = normalizedFrame->wdata();
    const size_t outStride = normalizedFrame->getStride();
    const uint32_t width = getWidth();
    for (uint32_t h = 0; h < getHeight(); h++, srcPtr += srcStride, outPtr += outStride) {
      const uint8_t* lineSrcPtr = srcPtr;
      uint8_t* lineOutPtr = outPtr;
      for (uint32_t w = 0; w < width; w++, lineSrcPtr++, lineOutPtr += 3) {
        lineOutPtr[0] = lineSrcPtr[0];
        lineOutPtr[1] = lineSrcPtr[0];
        lineOutPtr[2] = lineSrcPtr[0];
      }
    }
  } else if (srcFormat == PixelFormat::YUY2) {
    // Unoptimized default version of YUY2 to RGB8 conversion
    const uint8_t* srcPtr = rdata();
    const size_t srcStride = getStride();
    uint8_t* outPtr = normalizedFrame->wdata();
    const size_t outStride = normalizedFrame->getStride();
    const uint32_t width = getWidth();
    for (uint32_t h = 0; h < getHeight(); h++, srcPtr += srcStride, outPtr += outStride) {
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
  } else if (format == PixelFormat::GREY16 && bitsToShift > 0) {
    // 12/10 bit pixel scaling to 16 bit
    const uint16_t* srcPtr = reinterpret_cast<const uint16_t*>(rdata());
    uint16_t* outPtr = reinterpret_cast<uint16_t*>(normalizedFrame->wdata());
    const uint32_t pixelCount = getWidth() * getHeight() * componentCount;
    for (uint32_t i = 0; i < pixelCount; ++i) {
      outPtr[i] = static_cast<uint16_t>(srcPtr[i] << bitsToShift);
    }
  } else if (XR_VERIFY(this->size() == 2 * normalizedFrame->size())) {
    // 16/12/10 bit pixel reduction to 8 bit
    const uint16_t* srcPtr = reinterpret_cast<const uint16_t*>(rdata());
    uint8_t* outPtr = normalizedFrame->wdata();
    const uint32_t pixelCount = getWidth() * getHeight() * componentCount;
    for (uint32_t i = 0; i < pixelCount; ++i) {
      outPtr[i] = (srcPtr[i] >> bitsToShift) & 0xFF;
    }
  }
  return true;
}

#if IS_VRS_OSS_CODE()

bool PixelFrame::normalizeToPixelFormat(
    shared_ptr<PixelFrame>& convertedFrame,
    PixelFormat targetPixelFormat) const {
  return false;
}

bool PixelFrame::ssimCompare(const PixelFrame& other, double& meanSSIM, double& meanContrast) {
  XR_LOGE_ONCE("PixelFrame::ssimCompare has no open source implementation");
}

#endif // !IS_VRS_OSS_CODE()

} // namespace vrs::utils
