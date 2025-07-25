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

#include "DataLayoutConventions.h"

#include "RecordFormat.h"

namespace vrs {
namespace datalayout_conventions {

ContentBlock ImageSpec::getImageContentBlock(const ImageContentBlockSpec& base, size_t blockSize) {
  PixelFormat readPixelFormat{PixelFormat::UNDEFINED};
  uint32_t readWidth{};
  uint32_t readHeight{};
  uint32_t readBytesPerPixel{};
  uint8_t readBytesPerPixel8{};
  if (width.get(readWidth) && readWidth > 0 && height.get(readHeight) && readHeight > 0) {
    if (pixelFormat.get(readPixelFormat) && readPixelFormat != PixelFormat::UNDEFINED &&
        readPixelFormat < PixelFormat::COUNT) {
      // that's all we need
    } else {
      readPixelFormat = PixelFormat::UNDEFINED;
      if ((bytesPerPixels.get(readBytesPerPixel) && readBytesPerPixel > 0) ||
          (bytesPerPixels8.get(readBytesPerPixel8) &&
           (readBytesPerPixel = readBytesPerPixel8) > 0)) {
        // Legacy spec, without pixel format, so we have to make assumptions here
        switch (readBytesPerPixel) {
          case 1:
            readPixelFormat = PixelFormat::GREY8;
            break;
          case 3:
            readPixelFormat = PixelFormat::RGB8;
            break;
          case 4:
            readPixelFormat = PixelFormat::DEPTH32F;
            break;
          case 8:
            readPixelFormat = PixelFormat::SCALAR64F;
            break;
          default:
            break;
        }
      }
    }
  }
  bool hasMinRawSpec =
      readWidth != 0 && readHeight != 0 && readPixelFormat != PixelFormat::UNDEFINED;
  ImageFormat imageFormat = base.getImageFormat();
  if (imageFormat == ImageFormat::RAW) {
    if (hasMinRawSpec) {
      return {readPixelFormat, readWidth, readHeight, stride.get(), stride2.get()};
    }
  } else if (
      blockSize != ContentBlock::kSizeUnknown &&
      (imageFormat == ImageFormat::CUSTOM_CODEC || imageFormat == ImageFormat::VIDEO)) {
    string theCodecName;
    bool foundCodecName = codecName.get(theCodecName) && !theCodecName.empty();
    if (!foundCodecName) {
      theCodecName = base.getCodecName();
    }
    uint32_t quality = ImageContentBlockSpec::kQualityUndefined;
    if (!codecQuality.get(quality) || !ImageContentBlockSpec::isQualityValid(quality)) {
      quality = base.getCodecQuality();
    }
    if (imageFormat == ImageFormat::VIDEO) {
      if (!theCodecName.empty() && hasMinRawSpec) {
        return {
            ImageContentBlockSpec(
                std::move(theCodecName),
                quality,
                readPixelFormat,
                readWidth,
                readHeight,
                stride.get(),
                stride2.get()),
            blockSize};
      }
    } else if (imageFormat == ImageFormat::CUSTOM_CODEC) {
      if (foundCodecName || (!theCodecName.empty() && hasMinRawSpec)) {
        return {
            ImageContentBlockSpec(
                ImageFormat::CUSTOM_CODEC,
                readPixelFormat,
                readWidth,
                readHeight,
                stride.get(),
                stride2.get(),
                std::move(theCodecName),
                quality),
            blockSize};
      }
    }
  }
  return {};
}

} // namespace datalayout_conventions
} // namespace vrs
