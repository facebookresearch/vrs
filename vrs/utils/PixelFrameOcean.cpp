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

Ocean::FrameType::PixelFormat vrsToOceanPixelFormat(vrs::PixelFormat targetPixelFormat) {
  switch (targetPixelFormat) {
    case PixelFormat::GREY8:
      return Ocean::FrameType::FORMAT_Y8;
    case PixelFormat::GREY16:
      return Ocean::FrameType::FORMAT_Y16;
    default:
      return Ocean::FrameType::FORMAT_RGB24;
  }
}

} // namespace

namespace vrs::utils {

bool PixelFrame::msssimCompare(const PixelFrame& other, double& msssim) {
  if (!XR_VERIFY(getPixelFormat() == other.getPixelFormat()) ||
      !XR_VERIFY(getPixelFormat() == PixelFormat::RGB8 || getPixelFormat() == PixelFormat::GREY8) ||
      !XR_VERIFY(getWidth() == other.getWidth()) || !XR_VERIFY(getHeight() == other.getHeight())) {
    return false;
  }
  return XR_VERIFY(Ocean::CV::ImageQuality::multiScaleStructuralSimilarity8BitPerChannel(
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
  // Try to create an Ocean-style source frame
  unique_ptr<Ocean::Frame> sourceFrame;
  switch (imageSpec_.getPixelFormat()) {
    case vrs::PixelFormat::YUV_I420_SPLIT: {
      const FrameType sourceFrameType(
          width, height, FrameType::FORMAT_Y_U_V12, FrameType::ORIGIN_UPPER_LEFT);
      const uint8_t* baseAddressYPlane = rdata();
      const uint8_t* baseAddressUPlane =
          baseAddressYPlane + imageSpec_.getPlaneStride(0) * imageSpec_.getPlaneHeight(0);
      const uint8_t* baseAddressVPlane =
          baseAddressUPlane + imageSpec_.getPlaneStride(1) * imageSpec_.getPlaneHeight(1);
      Frame::PlaneInitializers<uint8_t> planeInitializers = {
          {baseAddressYPlane,
           Frame::CM_USE_KEEP_LAYOUT,
           imageSpec_.getPlaneStride(0) - imageSpec_.getDefaultStride()},
          {baseAddressUPlane,
           Frame::CM_USE_KEEP_LAYOUT,
           imageSpec_.getPlaneStride(1) - imageSpec_.getDefaultStride2()},
          {baseAddressVPlane,
           Frame::CM_USE_KEEP_LAYOUT,
           imageSpec_.getPlaneStride(2) - imageSpec_.getDefaultStride2()},
      };
      sourceFrame = make_unique<Frame>(sourceFrameType, planeInitializers);
    } break;
    case vrs::PixelFormat::YUV_420_NV21: {
      const FrameType sourceFrameType(
          width, height, FrameType::FORMAT_Y_VU12, FrameType::ORIGIN_UPPER_LEFT);
      const uint8_t* baseAddressYPlane = rdata();
      const uint8_t* baseAddressUPlane =
          baseAddressYPlane + imageSpec_.getPlaneStride(0) * imageSpec_.getPlaneHeight(0);
      Frame::PlaneInitializers<uint8_t> planeInitializers = {
          {baseAddressYPlane,
           Frame::CM_USE_KEEP_LAYOUT,
           imageSpec_.getPlaneStride(0) - imageSpec_.getDefaultStride()},
          {baseAddressUPlane,
           Frame::CM_USE_KEEP_LAYOUT,
           imageSpec_.getPlaneStride(1) - imageSpec_.getDefaultStride2()},
      };
      sourceFrame = make_unique<Frame>(sourceFrameType, planeInitializers);
    } break;
    case vrs::PixelFormat::YUV_420_NV12: {
      const FrameType sourceFrameType(
          width, height, FrameType::FORMAT_Y_UV12, FrameType::ORIGIN_UPPER_LEFT);
      const uint8_t* baseAddressYPlane = rdata();
      const uint8_t* baseAddressUPlane =
          baseAddressYPlane + imageSpec_.getPlaneStride(0) * imageSpec_.getPlaneHeight(0);
      Frame::PlaneInitializers<uint8_t> planeInitializers = {
          {baseAddressYPlane,
           Frame::CM_USE_KEEP_LAYOUT,
           imageSpec_.getPlaneStride(0) - imageSpec_.getDefaultStride()},
          {baseAddressUPlane,
           Frame::CM_USE_KEEP_LAYOUT,
           imageSpec_.getPlaneStride(1) - imageSpec_.getDefaultStride2()},
      };
      sourceFrame = make_unique<Frame>(sourceFrameType, planeInitializers);
    } break;
    case PixelFormat::YUY2: {
      const FrameType sourceFrameType(
          width, height, FrameType::FORMAT_YUYV16, FrameType::ORIGIN_UPPER_LEFT);
      sourceFrame = make_unique<Frame>(
          sourceFrameType, rdata(), Frame::CM_USE_KEEP_LAYOUT, imageSpec_.getStride() - 2 * width);
    } break;
    default:
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
