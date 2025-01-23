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

#include <cstring>

#include <algorithm>
#include <unordered_set>

#define DEFAULT_LOG_CHANNEL "PixelFrame"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/RecordFileReader.h>
#include <vrs/TagConventions.h>
#include <vrs/helpers/FileMacros.h>
#include <vrs/helpers/Throttler.h>
#include <vrs/utils/BufferRecordReader.hpp>
#include <vrs/utils/converters/Raw10ToGrey10Converter.h>

using namespace std;
using namespace vrs;

namespace {

utils::Throttler& getThrottler() {
  static utils::Throttler sThrottler;
  return sThrottler;
}

const uint8_t kNaNPixel = 0;

/// normalize float or double to grey8, with dynamic range calculation
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

/// normalize float to grey8, with interpolation for provided range, which values might exceed
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

mutex& getUsedColorsMutex() {
  static mutex sMutex;
  return sMutex;
}
set<uint16_t>& getUsedClassColors() {
  static set<uint16_t> sUsedClassColors;
  return sUsedClassColors;
}
set<uint16_t>& getusedObjectColors() {
  static set<uint16_t> sUsedObjectColors;
  return sUsedObjectColors;
};

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
  if (size != ContentBlock::kSizeUnknown) {
    frameBytes_.resize(size);
  }
}

PixelFrame::PixelFrame(PixelFormat pf, uint32_t w, uint32_t h, uint32_t stride)
    : imageSpec_{pf, w, h, stride, stride} {
  size_t size = imageSpec_.getRawImageSize();
  if (size != ContentBlock::kSizeUnknown) {
    frameBytes_.resize(size);
  }
}

PixelFrame::PixelFrame(const ImageContentBlockSpec& spec, vector<uint8_t>&& frameBytes)
    : imageSpec_{spec}, frameBytes_{std::move(frameBytes)} {}

void PixelFrame::init(const ImageContentBlockSpec& spec) {
  if (imageSpec_.getImageFormat() != ImageFormat::RAW || !hasSamePixels(spec)) {
    imageSpec_ = {
        spec.getPixelFormat(),
        spec.getWidth(),
        spec.getHeight(),
        spec.getRawStride(),
        spec.getRawStride2()};
    size_t size = imageSpec_.getRawImageSize();
    if (size != ContentBlock::kSizeUnknown) {
      frameBytes_.resize(size);
    }
  }
}

void PixelFrame::init(const ImageContentBlockSpec& spec, vector<uint8_t>&& frameBytes) {
  imageSpec_ = spec;
  frameBytes_ = std::move(frameBytes);
}

void PixelFrame::init(PixelFormat pf, uint32_t w, uint32_t h, uint32_t stride, uint32_t stride2) {
  imageSpec_ = {pf, w, h, stride, stride2};
  size_t size = imageSpec_.getRawImageSize();
  if (size != ContentBlock::kSizeUnknown) {
    frameBytes_.resize(size);
  }
}

void PixelFrame::swap(PixelFrame& other) noexcept {
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

bool PixelFrame::readFrame(RecordReader* reader, const ContentBlock& cb) {
  if (!XR_VERIFY(cb.getContentType() == ContentType::IMAGE)) {
    return false;
  }
  if (cb.image().getImageFormat() == ImageFormat::VIDEO) {
    return false;
  }
  switch (cb.image().getImageFormat()) {
    case ImageFormat::RAW:
      return readRawFrame(reader, cb.image());
    case ImageFormat::PNG:
      return readPngFrame(reader, cb.getBlockSize());
    case ImageFormat::JPG:
      return readJpegFrame(reader, cb.getBlockSize());
    case ImageFormat::JXL:
      return readJxlFrame(reader, cb.getBlockSize());
    default:
      return false;
  }
  return false;
}

bool PixelFrame::readDiskImageData(RecordReader* reader, const ContentBlock& cb) {
  size_t blockSize = cb.getBlockSize();
  if (cb.getContentType() != ContentType::IMAGE || blockSize == ContentBlock::kSizeUnknown) {
    return false;
  }
  const auto& spec = cb.image();
  if (spec.getImageFormat() == ImageFormat::RAW) {
    return readRawFrame(reader, spec);
  }
  imageSpec_ = spec;
  frameBytes_.resize(blockSize);
  return VERIFY_SUCCESS(reader->read(frameBytes_.data(), blockSize));
}

bool PixelFrame::decompressImage(VideoFrameHandler* videoFrameHandler) {
  switch (imageSpec_.getImageFormat()) {
    case ImageFormat::RAW:
      return true;
    case ImageFormat::VIDEO:
      if (videoFrameHandler != nullptr) {
        vector<uint8_t> compressedData(std::move(frameBytes_));
        BufferReader reader;
        return videoFrameHandler->tryToDecodeFrame(
                   *this, reader.init(compressedData), {imageSpec_, compressedData.size()}) == 0;
      }
      break;
    case ImageFormat::PNG: {
      vector<uint8_t> compressedData(std::move(frameBytes_));
      return readPngFrame(compressedData);
    }
    case ImageFormat::JPG: {
      vector<uint8_t> compressedData(std::move(frameBytes_));
      return readJpegFrame(compressedData);
    }
    case ImageFormat::JXL: {
      vector<uint8_t> compressedData(std::move(frameBytes_));
      return readJxlFrame(compressedData);
    }
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
        THROTTLED_LOGW(
            reader->getRef(),
            "Stride data missing for the last line. Please fix the recording app.");
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

void PixelFrame::normalizeFrame(
    const shared_ptr<PixelFrame>& sourceFrame,
    shared_ptr<PixelFrame>& outFrame,
    bool grey16supported,
    const NormalizeOptions& options) {
  if (!sourceFrame->normalizeFrame(outFrame, grey16supported, options)) {
    outFrame = sourceFrame;
  }
}

PixelFormat PixelFrame::getNormalizedPixelFormat(
    PixelFormat sourcePixelFormat,
    bool grey16supported,
    const NormalizeOptions& options) {
  PixelFormat format = sourcePixelFormat;
  if ((options.semantic == ImageSemantic::ObjectClassSegmentation ||
       options.semantic == ImageSemantic::ObjectIdSegmentation) &&
      sourcePixelFormat == PixelFormat::GREY16) {
    format = PixelFormat::RGB8;
#if IS_VRS_FB_INTERNAL()
  } else if (format == PixelFormat::BAYER8_RGGB) {
    format = PixelFormat::RGB8;
#endif
  } else {
    format = ImageContentBlockSpec::getChannelCountPerPixel(sourcePixelFormat) > 1
        ? PixelFormat::RGB8
        : grey16supported ? PixelFormat::GREY16
                          : PixelFormat::GREY8;
  }
  return format;
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

bool PixelFrame::normalizeFrame(
    shared_ptr<PixelFrame>& outNormalizedFrame,
    bool grey16supported,
    const NormalizeOptions& options) const {
  // See if we can convert to something simple enough using Ocean
  PixelFormat targetPixelFormat =
      getNormalizedPixelFormat(imageSpec_.getPixelFormat(), grey16supported, options);
  if (imageSpec_.getPixelFormat() == targetPixelFormat) {
    return false;
  }
  if (outNormalizedFrame.get() == this || !outNormalizedFrame) {
    outNormalizedFrame = make_shared<PixelFrame>();
  }
  return normalizeFrame(*outNormalizedFrame, grey16supported, options, targetPixelFormat);
}

bool PixelFrame::normalizeFrame(
    PixelFrame& outNormalizedFrame,
    bool grey16supported,
    const NormalizeOptions& options,
    PixelFormat normalizedPixelFormat) const {
  PixelFormat srcFormat = imageSpec_.getPixelFormat();
  if (normalizedPixelFormat == PixelFormat::UNDEFINED) {
    normalizedPixelFormat = getNormalizedPixelFormat(srcFormat, grey16supported, options);
  }
  if (options.semantic == ImageSemantic::Depth && getPixelFormat() == PixelFormat::DEPTH32F &&
      normalizedPixelFormat == PixelFormat::GREY8) {
    if (options.min < options.max) {
      outNormalizedFrame.init(normalizedPixelFormat, getWidth(), getHeight());
      normalizeBufferWithRange(
          rdata(), outNormalizedFrame.wdata(), getWidth() * getHeight(), options.min, options.max);
      return true;
    }
  } else if (
      (options.semantic == ImageSemantic::ObjectClassSegmentation ||
       options.semantic == ImageSemantic::ObjectIdSegmentation) &&
      getPixelFormat() == PixelFormat::GREY16 && normalizedPixelFormat == PixelFormat::RGB8) {
    outNormalizedFrame.init(normalizedPixelFormat, getWidth(), getHeight());
    bool classSegmentation = options.semantic == ImageSemantic::ObjectClassSegmentation;
    const vector<RGBColor>& colors = classSegmentation ? getGetObjectClassSegmentationColors()
                                                       : getGetObjectIdSegmentationColors();
    if (colors.size() >= (1UL << 16)) {
      unordered_set<uint16_t> usedColors;
      uint16_t lastColor = 0xffff;
      uint32_t srcStride = getStride();
      uint32_t dstStride = outNormalizedFrame.getStride();
      for (uint32_t h = 0; h < getHeight(); ++h) {
        const uint16_t* srcLine = data<uint16_t>(srcStride * h);
        RGBColor* dstLine = outNormalizedFrame.data<RGBColor>(dstStride * h);
        for (uint32_t w = 0; w < getWidth(); ++w) {
          uint16_t color = srcLine[w];
          dstLine[w] = colors[color];
          if (color != lastColor) {
            usedColors.insert(color);
            lastColor = color;
          }
        }
      }
      unique_lock<mutex> lock(getUsedColorsMutex());
      auto& sListedColors = classSegmentation ? ::getUsedClassColors() : ::getusedObjectColors();
      for (uint16_t color : usedColors) {
        sListedColors.insert(color);
      }
      return true;
    }
  }
  if (normalizeToPixelFormat(outNormalizedFrame, normalizedPixelFormat, options)) {
    return true;
  }
  PixelFormat format = srcFormat;
  uint16_t bitsToShift = 0;
  uint32_t componentCount = 0;
  switch (srcFormat) {
    case PixelFormat::YUV_I420_SPLIT:
    case PixelFormat::YUV_420_NV21:
    case PixelFormat::YUV_420_NV12: {
      // buffer truncation to grayscale when ocean isn't available
      const uint32_t width = imageSpec_.getWidth();
      const uint32_t height = imageSpec_.getHeight();
      const uint32_t stride = imageSpec_.getStride();
      outNormalizedFrame.init(PixelFormat::GREY8, width, height);
      const uint8_t* src = rdata();
      uint8_t* dst = outNormalizedFrame.wdata();
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
  outNormalizedFrame.init(format, getWidth(), getHeight());
  if (srcFormat == PixelFormat::BGR8) {
    // swap R & B
    const uint8_t* srcPtr = rdata();
    uint8_t* outPtr = outNormalizedFrame.wdata();
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
    normalizeRGBXfloatToRGB8(rdata(), outNormalizedFrame.wdata(), getWidth() * getHeight(), 3);
  } else if (srcFormat == PixelFormat::RGBA32F) {
    // normalize float pixels to rgb8, drop alpha channel
    normalizeRGBXfloatToRGB8(rdata(), outNormalizedFrame.wdata(), getWidth() * getHeight(), 4);
  } else if (srcFormat == PixelFormat::DEPTH32F) {
    // normalize float pixels to grey8
    normalizeBuffer<float>(rdata(), outNormalizedFrame.wdata(), getWidth() * getHeight());
  } else if (srcFormat == PixelFormat::SCALAR64F) {
    // normalize double pixels to grey8
    normalizeBuffer<double>(rdata(), outNormalizedFrame.wdata(), getWidth() * getHeight());
  } else if (srcFormat == PixelFormat::BAYER8_RGGB) {
    // display as grey8(copy) for now
    const uint8_t* srcPtr = rdata();
    uint8_t* outPtr = outNormalizedFrame.wdata();
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
              outNormalizedFrame.wdata(), rdata(), getWidth(), getHeight(), getStride())) {
        return false;
      }
      uint16_t* ptr = outNormalizedFrame.data<uint16_t>();
      const uint32_t pixelCount = getWidth() * getHeight() * componentCount;
      for (uint32_t i = 0; i < pixelCount; ++i) {
        ptr[i] <<= bitsToShift;
      }
    } else {
      // source: 4 bytes with 8 msb data, 1 byte with 4x 2 lsb of data
      // convert to GREY8 by copying 4 bytes of msb data, and dropping the 5th of lsb data...
      const uint8_t* srcPtr = rdata();
      const size_t srcStride = getStride();
      uint8_t* outPtr = outNormalizedFrame.wdata();
      const size_t outStride = outNormalizedFrame.getStride();
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
    uint8_t* outPtr = outNormalizedFrame.wdata();
    const size_t outStride = outNormalizedFrame.getStride();
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
    uint8_t* outPtr = outNormalizedFrame.wdata();
    const size_t outStride = outNormalizedFrame.getStride();
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
    const uint16_t* srcPtr = data<uint16_t>();
    uint16_t* outPtr = outNormalizedFrame.data<uint16_t>();
    const uint32_t pixelCount = getWidth() * getHeight() * componentCount;
    for (uint32_t i = 0; i < pixelCount; ++i) {
      outPtr[i] = static_cast<uint16_t>(srcPtr[i] << bitsToShift);
    }
  } else if (XR_VERIFY(this->size() == 2 * outNormalizedFrame.size())) {
    // 16/12/10 bit pixel reduction to 8 bit
    const uint16_t* srcPtr = data<uint16_t>();
    uint8_t* outPtr = outNormalizedFrame.wdata();
    const uint32_t pixelCount = getWidth() * getHeight() * componentCount;
    for (uint32_t i = 0; i < pixelCount; ++i) {
      outPtr[i] = (srcPtr[i] >> bitsToShift) & 0xFF;
    }
  }
  return true;
}

bool PixelFrame::psnrCompare(const PixelFrame& other, double& outPsnr) {
  if (!XR_VERIFY(getPixelFormat() == other.getPixelFormat()) ||
      !XR_VERIFY(getPixelFormat() == PixelFormat::RGB8 || getPixelFormat() == PixelFormat::GREY8) ||
      !XR_VERIFY(getWidth() == other.getWidth()) || !XR_VERIFY(getHeight() == other.getHeight())) {
    return false;
  }
  uint64_t err = 0;
  outPsnr = 100;
  uint32_t count = 0;
  const uint8_t* p1 = this->rdata();
  const uint8_t* p2 = other.rdata();
  for (uint32_t plane = 0; plane < imageSpec_.getPlaneCount(); ++plane) {
    uint32_t stride1 = imageSpec_.getPlaneStride(plane);
    uint32_t stride2 = other.imageSpec_.getPlaneStride(plane);
    // number of bytes to compare in this plane. For RGB, it's 3 bytes per pixel, for GREY8 it's 1
    uint32_t bytes = (plane == 0) ? imageSpec_.getDefaultStride() : imageSpec_.getDefaultStride2();
    uint32_t height = imageSpec_.getPlaneHeight(plane);
    for (uint32_t h = 0; h < height; ++h) {
      for (uint32_t w = 0; w < bytes; ++w) {
        int32_t d = static_cast<int32_t>(p1[w]) - p2[w];
        err += d * d;
      }
      p1 += stride1;
      p2 += stride2;
    }
    count += bytes * height;
  }
  if (err > 0) {
    double derr = static_cast<double>(err) / count;
    outPsnr = 10 * log10(255ll * 255 / derr);
  }
  return true;
}

inline uint8_t pToColor(uint32_t p, uint32_t parts) {
  return p > 0 ? p * (256 / parts) - 1 : 0;
}

// We want colors to be the same everywhere, so we need the same random numbers everywhere!
static uint32_t simple_random() {
  static uint32_t state{716172700};
  state = state * 1664525u + 1013904223u;
  return state;
}

// ...and the same shuffle algorithm. Fisher–Yates shuffle
static void shuffle(PixelFrame::RGBColor* colors, uint32_t count) {
  for (uint32_t i = count - 1; i >= 1; --i) {
    uint32_t j = simple_random() % (i + 1);
    swap(colors[i], colors[j]);
  }
}

/// This code builds a set of colors in successive batches, each with the most distinct colors
/// available, except for straight black & white.
/// 0 is black, and 0xffff is white, guaranteed.
/// The first batch doesn't split RGB bytes, giving us 2^3 - 1 colors (white excluded)
/// The second batch splits each RGB byte in 2 parts, giving us 3^3 - 8 colors: 25 colors total.
/// The third batch splits each RGB byte in 4 parts: 5^3 - 3^3 colors: 123 colors total.
/// The 4th batch splits each RGB byte in 8 parts: 9^3 - 5^3 colors: 727 colors total.
/// The 5th batch splits each RGB byte in 16 parts: 17^3 - 9^3 colors: 4911 colors total.
/// The 6th batch splits each RGB byte in 32 parts: 33^3 - 17^3 colors: 35935 colors total.
/// We randomize each batch, so colors are mixed up as much as possible.
/// In total, we generate 17^3 - white, or 35936 colors, which should be enough!
/// All the remaining values are black.
static vector<PixelFrame::RGBColor> makeObjectIdSegmentationColors() {
  const uint16_t lastBatch = 6;
  vector<PixelFrame::RGBColor> colors;
  constexpr size_t kMaxSize = 1 << 16;
  colors.reserve(kMaxSize);
  colors.emplace_back(0, 0, 0);
  uint32_t parts = 1;
  for (uint32_t batch = 1; batch <= lastBatch; ++batch, parts *= 2) {
    uint32_t previousSize = colors.size();
    uint32_t values = parts + 1;
    for (uint32_t r = 0; r < values; ++r) {
      uint8_t rv = pToColor(r, parts);
      for (uint32_t g = 0; g < values; ++g) {
        uint8_t gv = pToColor(g, parts);
        for (uint32_t b = 0; b < values; ++b) {
          if ((r & 1) + (g & 1) + (b & 1) != 0) {
            uint8_t bv = pToColor(b, parts);
            colors.emplace_back(rv, gv, bv);
          }
        }
      }
    }
    if (previousSize == 1) {
      colors.pop_back(); // remove white
    }
    shuffle(colors.data() + previousSize, colors.size() - previousSize);
  }
  colors.resize(kMaxSize);
  colors[kMaxSize - 1] = PixelFrame::RGBColor(255, 255, 255); // white
  return colors;
}

const vector<PixelFrame::RGBColor>& PixelFrame::getGetObjectIdSegmentationColors() {
  static const vector<PixelFrame::RGBColor> sColors = makeObjectIdSegmentationColors();
  return sColors;
}

const vector<PixelFrame::RGBColor>& PixelFrame::getGetObjectClassSegmentationColors() {
  return getGetObjectIdSegmentationColors();
}

static bool printSegColors(
    const set<uint16_t>& usedColors,
    const vector<PixelFrame::RGBColor>& colors,
    bool classSegmentation) {
  if (usedColors.empty()) {
    return false;
  }
  fmt::print("{} Segmentation Colors\n", classSegmentation ? "Class/Category" : "Object");
  vector<uint16_t> sortedColors(usedColors.size());
  uint32_t index = 0;
  for (uint16_t colorIndex : usedColors) {
    sortedColors[index++] = colorIndex;
  }
  string line;
  line.reserve(300);
  const uint32_t kMaxColumnIndex = classSegmentation ? 4 : 8;
  const uint32_t rows = (usedColors.size() + kMaxColumnIndex - 1) / kMaxColumnIndex;
  for (uint32_t row = 0; row < rows; ++row) {
    for (uint16_t column = 0; column < kMaxColumnIndex; ++column) {
      uint32_t colorIndex = rows * column + row;
      if (colorIndex < sortedColors.size()) {
        uint32_t color = sortedColors[colorIndex];
        PixelFrame::RGBColor c = colors[color];
        if (classSegmentation) {
          const char* className = PixelFrame::getSegmentationClassName(color);
          line.append(fmt::format(
              "{:>3} \x1b[48;2;{};{};{}m      \x1b[0m {:<25}", color, c.r, c.g, c.b, className));
        } else {
          line.append(fmt::format("\x1b[48;2;{};{};{}m      \x1b[0m {:<7}", c.r, c.g, c.b, color));
        }
      }
    }
    fmt::print("{}\n", line);
    line.clear();
  }
  fmt::print("\n");
  fflush(stdout);
  return true;
}

void PixelFrame::printSegmentationColors() {
  set<uint16_t> usedClassColors;
  set<uint16_t> usedObjectColors;
  {
    unique_lock<mutex> lock(getUsedColorsMutex());
    usedClassColors = getUsedClassColors();
    usedObjectColors = getusedObjectColors();
  }
  bool printed = printSegColors(usedClassColors, getGetObjectClassSegmentationColors(), true);
  printed |= printSegColors(usedObjectColors, getGetObjectIdSegmentationColors(), false);
  if (!printed) {
    fmt::print("No segmentation colors used.\n");
  }
}

void PixelFrame::clearSegmentationColors() {
  unique_lock<mutex> lock(getUsedColorsMutex());
  getUsedClassColors().clear();
  getusedObjectColors().clear();
}

static float asFloat(const string& strFloat, float defaultValue) {
  if (strFloat.empty()) {
    return defaultValue;
  }
  try {
    return stod(strFloat);
  } catch (logic_error&) {
    return defaultValue;
  }
}

static const float kDefaultDepthMin = 0;
static const float kDefaultDepthMax = 6;

NormalizeOptions
PixelFrame::getStreamNormalizeOptions(RecordFileReader& reader, StreamId id, PixelFormat format) {
  string imageSemantic = reader.getTag(id, tag_conventions::kImageSemantic);
  if (!imageSemantic.empty()) {
    if (imageSemantic == tag_conventions::kImageSemanticObjectClassSegmentation) {
      return NormalizeOptions(ImageSemantic::ObjectClassSegmentation);
    } else if (imageSemantic == tag_conventions::kImageSemanticObjectIdSegmentation) {
      return NormalizeOptions(ImageSemantic::ObjectIdSegmentation);
    } else if (imageSemantic == tag_conventions::kImageSemanticDepth) {
      float min =
          asFloat(reader.getTag(tag_conventions::kRenderDepthImagesRangeMin), kDefaultDepthMin);
      float max =
          asFloat(reader.getTag(tag_conventions::kRenderDepthImagesRangeMax), kDefaultDepthMax);
      return {ImageSemantic::Depth, min, max};
    } else if (imageSemantic == tag_conventions::kImageSemanticCamera) {
      return NormalizeOptions(ImageSemantic::Camera);
    }
  }
  /// Legacy streams handling, using RecordableTypeId as proxy
  switch (id.getTypeId()) {
    case RecordableTypeId::DepthCameraRecordableClass:
    case RecordableTypeId::GroundTruthDepthRecordableClass:
    case RecordableTypeId::RgbCameraRecordableClass:
    case RecordableTypeId::GroundTruthRecordableClass:
      if (format == PixelFormat::DEPTH32F) {
        return {ImageSemantic::Depth, kDefaultDepthMin, kDefaultDepthMax};
      } else if (format == PixelFormat::GREY16) {
        const string& flavor = reader.getFlavor(id);
        if (!flavor.empty()) {
          // Yes, the flavor names are counter intuitive, but...
          if (flavor.find("SegmentationObjectID") != string::npos) {
            return NormalizeOptions(ImageSemantic::ObjectClassSegmentation);
          } else if (flavor.find("SegmentationInstanceID") != string::npos) {
            return NormalizeOptions(ImageSemantic::ObjectIdSegmentation);
          }
        }
        return NormalizeOptions(ImageSemantic::ObjectIdSegmentation);
      }
      break;
    default:
      break;
  }
  return NormalizeOptions(ImageSemantic::Camera);
}

#if IS_VRS_OSS_CODE()

bool PixelFrame::normalizeToPixelFormat(
    PixelFrame& convertedFrame,
    PixelFormat targetPixelFormat,
    const NormalizeOptions& options) const {
  return false;
}

bool PixelFrame::msssimCompare(const PixelFrame& other, double& msssim) {
  THROTTLED_LOGW(nullptr, "PixelFrame::msssimCompare() has no open source implementation");
  return false;
}

const char* PixelFrame::getSegmentationClassName(uint16_t classIndex) {
  return "???";
}

#endif // !IS_VRS_OSS_CODE()

} // namespace vrs::utils
