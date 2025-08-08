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

#include "PixelFrameOptions.h"

#include <cmath>

#include <ocean/base/Frame.h>
#include <ocean/cv/FrameInterpolator.h>

#include "PixelFrame.h"

namespace {

inline uint32_t alignValue(float value, uint32_t alignment) {
  uint32_t uintValue = static_cast<uint32_t>(lround(value));
  return alignment > 1 ? ((uintValue + alignment - 1) / alignment) * alignment : uintValue;
}

} // namespace

namespace vrs::utils {

ResizeOptions ResizeOptions::withRatio(float ratio) {
  ResizeOptions options;
  options.mode = Mode::Ratio;
  options.ratio = ratio;
  return options;
}

ResizeOptions ResizeOptions::withResolution(uint32_t width, uint32_t height) {
  ResizeOptions options;
  options.mode = Mode::Resolution;
  options.targetWidth = width;
  options.targetHeight = height;
  return options;
}

bool ResizeOptions::computeTargetDimensions(
    uint32_t sourceWidth,
    uint32_t sourceHeight,
    uint32_t& outTargetWidth,
    uint32_t& outTargetHeight) const {
  switch (mode) {
    case Mode::None:
      return false; // No scaling requested

    case Mode::Ratio: {
      if (ratio <= 0.0f || ratio == 1.0f || sourceWidth == 0 || sourceHeight == 0) {
        return false; // Invalid ratio or no scaling needed or possible
      }
      outTargetWidth = alignValue(sourceWidth * ratio, widthAlignment);
      // Calculate proportional height, taking into account width alignment for accurate ratio
      float aspectRatio = static_cast<float>(sourceHeight) / static_cast<float>(sourceWidth);
      outTargetHeight = alignValue(outTargetWidth * aspectRatio, heightAlignment);
      break;
    }

    case Mode::Resolution: {
      if (targetWidth == 0 && targetHeight == 0) {
        return false; // Both dimensions must not be zero
      }

      if (targetWidth > 0 && targetHeight > 0) {
        // Both dimensions specified - use exact resolution
        outTargetWidth = targetWidth;
        outTargetHeight = targetHeight;
      } else if (sourceWidth == 0 || sourceHeight == 0) {
        return false; // Invalid source dimensions
      } else if (targetWidth > 0) {
        // Only width specified - calculate proportional height
        float aspectRatio = static_cast<float>(sourceHeight) / static_cast<float>(sourceWidth);
        outTargetWidth = targetWidth;
        outTargetHeight = alignValue(targetWidth * aspectRatio, heightAlignment);
      } else if (targetHeight > 0) {
        // Only height specified - calculate proportional width
        float aspectRatio = static_cast<float>(sourceWidth) / static_cast<float>(sourceHeight);
        outTargetWidth = alignValue(targetHeight * aspectRatio, widthAlignment);
        outTargetHeight = targetHeight;
      }
      break;
    }

    default:
      return false;
  }

  return outTargetWidth != sourceWidth || outTargetHeight != sourceHeight;
}

std::unique_ptr<PixelFrame> ResizeOptions::rescale(const PixelFrame& sourceFrame) const {
  const ImageContentBlockSpec& sourceSpec = sourceFrame.getSpec();
  uint32_t sourceWidth = sourceSpec.getWidth();
  uint32_t sourceHeight = sourceSpec.getHeight();

  uint32_t computedWidth = 0, computedHeight = 0;
  if (!computeTargetDimensions(sourceWidth, sourceHeight, computedWidth, computedHeight)) {
    return nullptr;
  }

  // Only support GREY8 and RGB8 pixel formats for downscaling
  PixelFormat pixelFormat = sourceSpec.getPixelFormat();
  if (pixelFormat != PixelFormat::GREY8 && pixelFormat != PixelFormat::RGB8) {
    return nullptr;
  }

  std::unique_ptr<PixelFrame> targetFrame =
      std::make_unique<PixelFrame>(pixelFormat, computedWidth, computedHeight);

  const Ocean::FrameType::PixelFormat oceanPixelFormat = (pixelFormat == PixelFormat::GREY8)
      ? Ocean::FrameType::PixelFormat::FORMAT_Y8
      : Ocean::FrameType::PixelFormat::FORMAT_RGB24;

  unsigned int sourcePaddingElements{};
  if (!Ocean::Frame::strideBytes2paddingElements(
          oceanPixelFormat, sourceWidth, sourceSpec.getStride(), sourcePaddingElements)) {
    return nullptr;
  }

  Ocean::Frame::PlaneInitializers<uint8_t> sourcePlaneInitializers;
  for (uint32_t i = 0; i < sourceSpec.getPlaneCount(); ++i) {
    sourcePlaneInitializers.emplace_back(
        const_cast<uint8_t*>(sourceFrame.rdata()),
        Ocean::Frame::CM_USE_KEEP_LAYOUT,
        sourcePaddingElements);
  }

  Ocean::FrameType sourceFrameType(
      sourceWidth, sourceHeight, oceanPixelFormat, Ocean::FrameType::ORIGIN_UPPER_LEFT);
  const Ocean::Frame sourceOceanFrame(sourceFrameType, sourcePlaneInitializers);

  const ImageContentBlockSpec& targetSpec = targetFrame->getSpec();
  unsigned int targetPaddingElements{};
  if (!Ocean::Frame::strideBytes2paddingElements(
          oceanPixelFormat, computedWidth, targetSpec.getStride(), targetPaddingElements)) {
    return nullptr;
  }

  Ocean::Frame::PlaneInitializers<uint8_t> targetPlaneInitializers;
  for (uint32_t i = 0; i < targetSpec.getPlaneCount(); ++i) {
    targetPlaneInitializers.emplace_back(
        targetFrame->wdata(), Ocean::Frame::CM_USE_KEEP_LAYOUT, targetPaddingElements);
  }

  Ocean::FrameType targetFrameType(
      computedWidth, computedHeight, oceanPixelFormat, Ocean::FrameType::ORIGIN_UPPER_LEFT);
  Ocean::Frame targetOceanFrame(targetFrameType, targetPlaneInitializers);

  if (!Ocean::CV::FrameInterpolator::resize(
          sourceOceanFrame,
          targetOceanFrame,
          Ocean::CV::FrameInterpolator::ResizeMethod::RM_AUTOMATIC)) {
    return nullptr;
  }

  return targetFrame;
}

} // namespace vrs::utils
