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

#include <vrs/utils/PixelFrame.h>
#include <vrs/utils/PixelFrameOcean.h>

#include <ocean/base/Frame.h>
#include <ocean/base/WorkerPool.h>
#include <ocean/cv/FrameConverter.h>
#include <ocean/cv/ImageQuality.h>

#define DEFAULT_LOG_CHANNEL "PixelFrameOcean"
#include <logging/Log.h>
#include <logging/Verify.h>

using namespace std;
using namespace vrs;

namespace {
/// Create an Ocean::Frame from a VRS PixelFrame with proper plane initialization
template <typename dataptr>
std::unique_ptr<Ocean::Frame> createOceanFrame(
    const ImageContentBlockSpec& imageSpec,
    dataptr data,
    Ocean::FrameType::PixelFormat oceanPixelFormat) {
  const uint32_t width = imageSpec.getWidth();
  const uint32_t height = imageSpec.getHeight();

  switch (imageSpec.getPixelFormat()) {
    case vrs::PixelFormat::YUV_I420_SPLIT: {
      const Ocean::FrameType frameType(
          width, height, Ocean::FrameType::FORMAT_Y_U_V12, Ocean::FrameType::ORIGIN_UPPER_LEFT);
      dataptr baseAddressYPlane = data;
      dataptr baseAddressUPlane =
          baseAddressYPlane + imageSpec.getPlaneStride(0) * imageSpec.getPlaneHeight(0);
      dataptr baseAddressVPlane =
          baseAddressUPlane + imageSpec.getPlaneStride(1) * imageSpec.getPlaneHeight(1);
      Ocean::Frame::PlaneInitializers<uint8_t> planeInitializers = {
          {baseAddressYPlane,
           Ocean::Frame::CM_USE_KEEP_LAYOUT,
           imageSpec.getPlaneStride(0) - imageSpec.getDefaultStride()},
          {baseAddressUPlane,
           Ocean::Frame::CM_USE_KEEP_LAYOUT,
           imageSpec.getPlaneStride(1) - imageSpec.getDefaultStride2()},
          {baseAddressVPlane,
           Ocean::Frame::CM_USE_KEEP_LAYOUT,
           imageSpec.getPlaneStride(2) - imageSpec.getDefaultStride2()},
      };
      return std::make_unique<Ocean::Frame>(frameType, planeInitializers);
    }
    case vrs::PixelFormat::YUV_420_NV21: {
      const Ocean::FrameType frameType(
          width, height, Ocean::FrameType::FORMAT_Y_VU12, Ocean::FrameType::ORIGIN_UPPER_LEFT);
      dataptr baseAddressYPlane = data;
      dataptr baseAddressUPlane =
          baseAddressYPlane + imageSpec.getPlaneStride(0) * imageSpec.getPlaneHeight(0);
      Ocean::Frame::PlaneInitializers<uint8_t> planeInitializers = {
          {baseAddressYPlane,
           Ocean::Frame::CM_USE_KEEP_LAYOUT,
           imageSpec.getPlaneStride(0) - imageSpec.getDefaultStride()},
          {baseAddressUPlane,
           Ocean::Frame::CM_USE_KEEP_LAYOUT,
           imageSpec.getPlaneStride(1) - imageSpec.getDefaultStride2()},
      };
      return std::make_unique<Ocean::Frame>(frameType, planeInitializers);
    }
    case vrs::PixelFormat::YUV_420_NV12: {
      const Ocean::FrameType frameType(
          width, height, Ocean::FrameType::FORMAT_Y_UV12, Ocean::FrameType::ORIGIN_UPPER_LEFT);
      dataptr baseAddressYPlane = data;
      dataptr baseAddressUPlane =
          baseAddressYPlane + imageSpec.getPlaneStride(0) * imageSpec.getPlaneHeight(0);
      Ocean::Frame::PlaneInitializers<uint8_t> planeInitializers = {
          {baseAddressYPlane,
           Ocean::Frame::CM_USE_KEEP_LAYOUT,
           imageSpec.getPlaneStride(0) - imageSpec.getDefaultStride()},
          {baseAddressUPlane,
           Ocean::Frame::CM_USE_KEEP_LAYOUT,
           imageSpec.getPlaneStride(1) - imageSpec.getDefaultStride2()},
      };
      return std::make_unique<Ocean::Frame>(frameType, planeInitializers);
    }
    case PixelFormat::YUY2: {
      const Ocean::FrameType frameType(
          width, height, Ocean::FrameType::FORMAT_YUYV16, Ocean::FrameType::ORIGIN_UPPER_LEFT);
      return std::make_unique<Ocean::Frame>(
          frameType, data, Ocean::Frame::CM_USE_KEEP_LAYOUT, imageSpec.getStride() - 2 * width);
    }
    default: {
      // For single-plane formats, use the provided Ocean pixel format
      const Ocean::FrameType frameType(
          width, height, oceanPixelFormat, Ocean::FrameType::ORIGIN_UPPER_LEFT);
      unsigned int paddingElements = 0;
      if (Ocean::Frame::strideBytes2paddingElements(
              oceanPixelFormat, width, imageSpec.getStride(), paddingElements)) {
        return std::make_unique<Ocean::Frame>(
            frameType, data, Ocean::Frame::CM_USE_KEEP_LAYOUT, paddingElements);
      }
      return nullptr;
    }
  }
}
} // namespace

namespace vrs::utils {

std::unique_ptr<Ocean::Frame> createReadOnlyOceanFrame(
    const ImageContentBlockSpec& imageSpec,
    const uint8_t* data,
    Ocean::FrameType::PixelFormat oceanPixelFormat) {
  return createOceanFrame(imageSpec, data, oceanPixelFormat);
}

std::unique_ptr<Ocean::Frame> createWritableOceanFrame(
    const ImageContentBlockSpec& imageSpec,
    uint8_t* data,
    Ocean::FrameType::PixelFormat oceanPixelFormat) {
  return createOceanFrame(imageSpec, data, oceanPixelFormat);
}

/// All the VRS pixel formats to Ocean pixel formats we know, even if conversion is not supported.
Ocean::FrameType::PixelFormat vrsToOceanPixelFormat(vrs::PixelFormat targetPixelFormat) {
  switch (targetPixelFormat) {
    case vrs::PixelFormat::GREY8:
      return Ocean::FrameType::FORMAT_Y8;
    case vrs::PixelFormat::GREY10:
      return Ocean::FrameType::FORMAT_Y10;
    case vrs::PixelFormat::RAW10:
      return Ocean::FrameType::FORMAT_Y10_PACKED;
    case vrs::PixelFormat::GREY16:
      return Ocean::FrameType::FORMAT_Y16;
    case vrs::PixelFormat::RGB8:
      return Ocean::FrameType::FORMAT_RGB24;
    case vrs::PixelFormat::RGBA8:
      return Ocean::FrameType::FORMAT_RGBA32;
    case vrs::PixelFormat::YUY2:
      return Ocean::FrameType::FORMAT_YUYV16;
    case vrs::PixelFormat::YUV_I420_SPLIT:
      return Ocean::FrameType::FORMAT_Y_U_V12;
    case vrs::PixelFormat::YUV_420_NV21:
      return Ocean::FrameType::FORMAT_Y_VU12;
    case vrs::PixelFormat::YUV_420_NV12:
      return Ocean::FrameType::FORMAT_Y_UV12;
    default:
      return Ocean::FrameType::FORMAT_UNDEFINED;
  }
}

bool PixelFrame::msssimCompare(const PixelFrame& other, double& msssim) {
  if (!XR_VERIFY(getPixelFormat() == other.getPixelFormat()) ||
      !XR_VERIFY(getPixelFormat() == PixelFormat::RGB8 || getPixelFormat() == PixelFormat::GREY8) ||
      !XR_VERIFY(getWidth() == other.getWidth()) || !XR_VERIFY(getHeight() == other.getHeight())) {
    return false;
  }
  return XR_VERIFY(
      Ocean::CV::ImageQuality::multiScaleStructuralSimilarity8BitPerChannel(
          rdata(),
          other.rdata(),
          getWidth(),
          getHeight(),
          getChannelCountPerPixel(),
          getStride() - getDefaultStride(),
          other.getStride() - other.getDefaultStride(),
          msssim));
}

bool PixelFrame::normalizeToPixelFormatWithOcean(
    PixelFrame& outNormalizedFrame,
    PixelFormat targetPixelFormat,
    const NormalizeOptions& options) const {
  using namespace Ocean;
  const uint32_t width = imageSpec_.getWidth();
  const uint32_t height = imageSpec_.getHeight();

  // Use the helper function to create the source Ocean::Frame with proper plane initialization
  auto sourceFrame =
      createReadOnlyOceanFrame(imageSpec_, rdata(), Ocean::FrameType::FORMAT_UNDEFINED);
  if (!sourceFrame) {
    return false;
  }

  // Create an Ocean-style target frame
  outNormalizedFrame.init(targetPixelFormat, width, height);
  const FrameType targetFrameType(
      width, height, vrsToOceanPixelFormat(targetPixelFormat), FrameType::ORIGIN_UPPER_LEFT);
  Frame targetFrame(targetFrameType, outNormalizedFrame.wdata(), Frame::CM_USE_KEEP_LAYOUT);
  return CV::FrameConverter::Comfort::convert(
      *sourceFrame,
      targetFrameType.pixelFormat(),
      targetFrameType.pixelOrigin(),
      targetFrame,
      CV::FrameConverter::CP_ALWAYS_COPY,
      width * height >= 640 * 480 ? Ocean::WorkerPool::get().scopedWorker()() : nullptr);
}

} // namespace vrs::utils
