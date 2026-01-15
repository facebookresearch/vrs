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

#include <vrs/utils/PixelFrameOptions.h>

#include <cmath>

#define DEFAULT_LOG_CHANNEL "PixelFrameOptions"
#include <logging/Verify.h>

#include <ocean/base/Frame.h>
#include <ocean/cv/FrameInterpolator.h>

#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/PixelFrameOcean.h>

namespace {

inline uint32_t alignValue(float value, uint32_t alignment) {
  uint32_t uintValue = static_cast<uint32_t>(std::lround(value));
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

// Only the pixel formats for which we verified that Ocean supports resizing.
bool ResizeOptions::canResize(PixelFormat pixelFormat) {
  return pixelFormat == PixelFormat::GREY8 || pixelFormat == PixelFormat::RGB8 ||
      pixelFormat == PixelFormat::RGBA8 || pixelFormat == PixelFormat::YUY2 ||
      pixelFormat == PixelFormat::YUV_I420_SPLIT || pixelFormat == PixelFormat::YUV_420_NV21 ||
      pixelFormat == PixelFormat::YUV_420_NV12;
}

std::unique_ptr<PixelFrame> ResizeOptions::resize(const PixelFrame& sourceFrame) const {
  const ImageContentBlockSpec& sourceSpec = sourceFrame.getSpec();

  PixelFormat pixelFormat = sourceSpec.getPixelFormat();
  if (!canResize(pixelFormat)) {
    return nullptr;
  }
  const Ocean::FrameType::PixelFormat oceanPixelFormat = vrsToOceanPixelFormat(pixelFormat);
  if (oceanPixelFormat == Ocean::FrameType::FORMAT_UNDEFINED) {
    return nullptr;
  }

  uint32_t sourceWidth = sourceSpec.getWidth();
  uint32_t sourceHeight = sourceSpec.getHeight();

  uint32_t computedWidth = 0, computedHeight = 0;
  if (!computeTargetDimensions(sourceWidth, sourceHeight, computedWidth, computedHeight)) {
    return nullptr;
  }

  std::unique_ptr<PixelFrame> targetFrame =
      std::make_unique<PixelFrame>(pixelFormat, computedWidth, computedHeight);

  auto sourceOceanFrame =
      createReadOnlyOceanFrame(sourceSpec, sourceFrame.rdata(), oceanPixelFormat);
  if (!XR_VERIFY(sourceOceanFrame != nullptr, "Failed to create source Ocean frame")) {
    return nullptr;
  }

  const ImageContentBlockSpec& targetSpec = targetFrame->getSpec();
  auto targetOceanFrame =
      createWritableOceanFrame(targetSpec, targetFrame->wdata(), oceanPixelFormat);
  if (!XR_VERIFY(targetOceanFrame != nullptr, "Failed to create target Ocean frame")) {
    return nullptr;
  }

  if (!XR_VERIFY(
          Ocean::CV::FrameInterpolator::resize(
              *sourceOceanFrame,
              *targetOceanFrame,
              Ocean::CV::FrameInterpolator::ResizeMethod::RM_AUTOMATIC),
          "Failed to resize frame")) {
    return nullptr;
  }

  return targetFrame;
}

} // namespace vrs::utils
