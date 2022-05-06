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

#define DEFAULT_LOG_CHANNEL "PixelFrame"
#include <logging/Checks.h>
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/utils/converters/Raw10ToGrey10Converter.h>

using namespace std;

namespace {
/// normalize floating point pixels to grey8
template <class Float>
void normalizeBuffer(const uint8_t* pixelPtr, uint8_t* outPtr, uint32_t pixelCount) {
  const Float* srcPtr = reinterpret_cast<const Float*>(pixelPtr);
  Float min = *srcPtr;
  Float max = *srcPtr;
  for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
    const Float pixel = srcPtr[pixelIndex];
    if (pixel < min) {
      min = pixel;
    } else if (pixel > max) {
      max = pixel;
    }
  }
  if (min >= max) {
    // for constant input, blank the image
    memset(outPtr, 0, pixelCount);
  } else {
    const Float factor = numeric_limits<uint8_t>::max() / (max - min);
    for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
      outPtr[pixelIndex] = static_cast<uint8_t>((srcPtr[pixelIndex] - min) * factor);
    }
  }
}

void normalizeRGBXfloatToRGB8(
    const uint8_t* pixelPtr,
    uint8_t* outPtr,
    uint32_t pixelCount,
    size_t channelCount) {
  const float* srcPtr = reinterpret_cast<const float*>(pixelPtr);
  float minR = *srcPtr;
  float maxR = minR;
  float minG = srcPtr[1];
  float maxG = minG;
  float minB = srcPtr[2];
  float maxB = minB;
  for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex, srcPtr += channelCount) {
    float pixelR = srcPtr[0];
    if (pixelR < minR) {
      minR = pixelR;
    } else if (pixelR > maxR) {
      maxR = pixelR;
    }
    float pixelG = srcPtr[1];
    if (pixelG < minG) {
      minG = pixelG;
    } else if (pixelG > maxG) {
      maxG = pixelG;
    }
    float pixelB = srcPtr[2];
    if (pixelB < minB) {
      minB = pixelB;
    } else if (pixelB > maxB) {
      maxB = pixelB;
    }
  }
  const float factorR = maxR > minR ? numeric_limits<uint8_t>::max() / (maxR - minR) : 0;
  const float factorG = maxG > minG ? numeric_limits<uint8_t>::max() / (maxG - minG) : 0;
  const float factorB = maxB > minB ? numeric_limits<uint8_t>::max() / (maxB - minB) : 0;
  srcPtr = reinterpret_cast<const float*>(pixelPtr);
  for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
    *outPtr++ = static_cast<uint8_t>((srcPtr[0] - minR) * factorR);
    *outPtr++ = static_cast<uint8_t>((srcPtr[1] - minG) * factorG);
    *outPtr++ = static_cast<uint8_t>((srcPtr[2] - minB) * factorB);
    srcPtr += channelCount;
  }
}

} // namespace

namespace vrs::utils {

PixelFrame::PixelFrame(const ImageContentBlockSpec& spec)
    : imageSpec_{spec.getPixelFormat(), spec.getWidth(), spec.getHeight(), spec.getStride()} {
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
  memset(wdata(), 0, frameBytes_.size());
}

bool PixelFrame::readRawFrame(RecordReader* reader, const ImageContentBlockSpec& inputImageSpec) {
  // Read multiplane images as is
  if (inputImageSpec.getPlaneCount() != 1) {
    init(inputImageSpec);
    return XR_VERIFY(reader->read(wdata(), size()) == 0);
  }
  // remove stride of single plane raw images
  ImageContentBlockSpec noStrideSpec{
      inputImageSpec.getPixelFormat(), inputImageSpec.getWidth(), inputImageSpec.getHeight()};
  if (inputImageSpec.getStride() == noStrideSpec.getStride()) {
    init(inputImageSpec);
    return XR_VERIFY(reader->read(wdata(), size()) == 0);
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
    if (!XR_VERIFY(reader->read(wdata, frameStride) == 0)) {
      return false;
    }
    if (!strideGap.empty()) {
      if (!XR_VERIFY(reader->read(strideGap) == 0)) {
        return false;
      }
    }
    wdata += frameStride;
  }
  return true;
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
  return ImageContentBlockSpec::getChannelCountPerPixel(sourcePixelFormat) > 1
      ? vrs::PixelFormat::RGB8
      : grey16supported ? vrs::PixelFormat::GREY16
                        : vrs::PixelFormat::GREY8;
}

bool PixelFrame::normalizeFrame(shared_ptr<PixelFrame>& normalizedFrame, bool grey16supported)
    const {
  uint16_t bitsToShift = 0;
  uint32_t componentCount = 0;
  vrs::PixelFormat format = imageSpec_.getPixelFormat();
  // See if we can convert to something simple enough using Ocean
  vrs::PixelFormat targetPixelFormat =
      getNormalizedPixelFormat(imageSpec_.getPixelFormat(), grey16supported);
  if (format == targetPixelFormat) {
    return false;
  }
  if (normalizeToPixelFormat(normalizedFrame, targetPixelFormat)) {
    return true;
  }
  switch (format) {
    case vrs::PixelFormat::YUV_I420_SPLIT: {
      // buffer truncation to greyscale when ocean isn't available
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
    case vrs::PixelFormat::GREY10:
      if (grey16supported) {
        format = vrs::PixelFormat::GREY16;
        bitsToShift = 6;
        componentCount = 1;
      } else {
        format = vrs::PixelFormat::GREY8;
        bitsToShift = 2;
        componentCount = 1;
      }
      break;
    case vrs::PixelFormat::GREY12:
      if (grey16supported) {
        format = vrs::PixelFormat::GREY16;
        bitsToShift = 4;
        componentCount = 1;
      } else {
        format = vrs::PixelFormat::GREY8;
        bitsToShift = 4;
        componentCount = 1;
      }
      break;
    case vrs::PixelFormat::GREY16:
      if (grey16supported) {
        // nothing to do!
      } else {
        format = vrs::PixelFormat::GREY8;
        bitsToShift = 8;
        componentCount = 1;
      }
      break;
    case vrs::PixelFormat::RGB10:
      format = vrs::PixelFormat::RGB8;
      bitsToShift = 2;
      componentCount = 3;
      break;
    case vrs::PixelFormat::RGB12:
      format = vrs::PixelFormat::RGB8;
      bitsToShift = 4;
      componentCount = 3;
      break;
    case vrs::PixelFormat::BGR8:
      format = vrs::PixelFormat::RGB8;
      componentCount = 3;
      break;
    case vrs::PixelFormat::RGB32F:
      format = vrs::PixelFormat::RGB8;
      componentCount = 3;
      break;
    case vrs::PixelFormat::RGBA32F:
      format = vrs::PixelFormat::RGB8;
      componentCount = 3;
      break;
    case vrs::PixelFormat::DEPTH32F:
    case vrs::PixelFormat::SCALAR64F:
    case vrs::PixelFormat::BAYER8_RGGB:
      format = vrs::PixelFormat::GREY8;
      componentCount = 1;
      break;
    case vrs::PixelFormat::RGB_IR_RAW_4X4:
      format = vrs::PixelFormat::RGB8;
      componentCount = 1;
      break;
    case vrs::PixelFormat::RAW10:
    case vrs::PixelFormat::RAW10_BAYER_RGGB:
      if (grey16supported) {
        format = vrs::PixelFormat::GREY16;
        bitsToShift = 6;
        componentCount = 1;
      } else {
        format = vrs::PixelFormat::GREY8;
        bitsToShift = 2;
        componentCount = 1;
      }
      break;
    default:
      break;
  }
  if (format == imageSpec_.getPixelFormat()) {
    return false; // no conversion needed or supported
  }
  init(normalizedFrame, format, getWidth(), getHeight());
  if (imageSpec_.getPixelFormat() == vrs::PixelFormat::BGR8) {
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
  } else if (imageSpec_.getPixelFormat() == vrs::PixelFormat::RGB32F) {
    // normalize float pixels to rgb8
    normalizeRGBXfloatToRGB8(rdata(), normalizedFrame->wdata(), getWidth() * getHeight(), 3);
  } else if (imageSpec_.getPixelFormat() == vrs::PixelFormat::RGBA32F) {
    // normalize float pixels to rgb8, drop alpha channel
    normalizeRGBXfloatToRGB8(rdata(), normalizedFrame->wdata(), getWidth() * getHeight(), 4);
  } else if (imageSpec_.getPixelFormat() == vrs::PixelFormat::DEPTH32F) {
    // normalize float pixels to grey8
    normalizeBuffer<float>(rdata(), normalizedFrame->wdata(), getWidth() * getHeight());
  } else if (imageSpec_.getPixelFormat() == vrs::PixelFormat::SCALAR64F) {
    // normalize double pixels to grey8
    normalizeBuffer<double>(rdata(), normalizedFrame->wdata(), getWidth() * getHeight());
  } else if (imageSpec_.getPixelFormat() == vrs::PixelFormat::BAYER8_RGGB) {
    // display as grey8(copy) for now
    const uint8_t* srcPtr = rdata();
    uint8_t* outPtr = normalizedFrame->wdata();
    const uint32_t pixelCount = getWidth() * getHeight() * componentCount;
    for (uint32_t i = 0; i < pixelCount; ++i) {
      outPtr[i] = srcPtr[i];
    }
  } else if (
      imageSpec_.getPixelFormat() == vrs::PixelFormat::RAW10 ||
      imageSpec_.getPixelFormat() == vrs::PixelFormat::RAW10_BAYER_RGGB) {
    if (format == vrs::PixelFormat::GREY16) {
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
  } else if (imageSpec_.getPixelFormat() == vrs::PixelFormat::RGB_IR_RAW_4X4) {
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
  } else if (format == vrs::PixelFormat::GREY16 && bitsToShift > 0) {
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

#endif // !IS_VRS_OSS_CODE()

} // namespace vrs::utils
