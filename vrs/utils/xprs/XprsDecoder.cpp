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

#include <vrs/utils/xprs/XprsDecoder.h>

#include <cstring>

#include <algorithm>

#include <fmt/format.h>

#include <xprs.h>

#include <ocean/base/Frame.h>
#include <ocean/base/WorkerPool.h>
#include <ocean/cv/FrameConverter.h>
#include <ocean/cv/FrameConverterY_U_V12.h>

#define DEFAULT_LOG_CHANNEL "XprsDecoder"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/ErrorCode.h>
#include <vrs/TelemetryLogger.h>
#include <vrs/helpers/FileMacros.h>

namespace {
using namespace vrs;
PixelFormat xprsToVrsPixelFormat(
    xprs::PixelFormat xprsPixelFormat,
    vrs::PixelFormat vrsPixelFormat) {
  switch (xprsPixelFormat) {
    case xprs::PixelFormat::GRAY8:
      return PixelFormat::GREY8;
    case xprs::PixelFormat::GRAY10LE:
      return PixelFormat::GREY10;
    case xprs::PixelFormat::GRAY12LE:
      return PixelFormat::GREY12;
    case xprs::PixelFormat::RGB24:
    case xprs::PixelFormat::GBRP:
      return PixelFormat::RGB8;
    case xprs::PixelFormat::YUV420P:
      // some codecs don't support GREY8 or NV12, and will silently convert to YUV420
      if (vrsPixelFormat == PixelFormat::GREY8 || vrsPixelFormat == PixelFormat::RGB8 ||
          vrsPixelFormat == PixelFormat::YUV_420_NV12) {
        return vrsPixelFormat;
      }
      return PixelFormat::YUV_I420_SPLIT;
    case xprs::PixelFormat::NV12:
      if (vrsPixelFormat == PixelFormat::GREY8 || vrsPixelFormat == PixelFormat::YUV_I420_SPLIT ||
          vrsPixelFormat == PixelFormat::RGB8) {
        return vrsPixelFormat;
      }
      return PixelFormat::YUV_420_NV12;
    case xprs::PixelFormat::YUV444P:
      return PixelFormat::RGB8; // Using Ocean to convert!
    case xprs::PixelFormat::YUV420P10LE:
      // some codecs don't support GREY10, and will silently convert to YUV420P10LE
      if (vrsPixelFormat == PixelFormat::GREY10) {
        return PixelFormat::GREY10;
      }
      break;
    case xprs::PixelFormat::NV1210LE:
      if (vrsPixelFormat == PixelFormat::GREY10 || vrsPixelFormat == PixelFormat::RGB10) {
        return vrsPixelFormat;
      }
      break;
    case xprs::PixelFormat::NV1212LE:
      if (vrsPixelFormat == PixelFormat::GREY12 || vrsPixelFormat == PixelFormat::RGB12) {
        return vrsPixelFormat;
      }
      break;
    default:
      break;
  }
  return PixelFormat::UNDEFINED;
}

Ocean::Worker* getWorkers(uint32_t pixels) {
  return pixels >= 640 * 480 ? Ocean::WorkerPool::get().scopedWorker()() : nullptr;
}

} // namespace

namespace vrs::vxprs {

using namespace std;
using xprs::XprsResult;

namespace {
class VXDecoder : public vrs::utils::DecoderI {
 public:
  explicit VXDecoder(unique_ptr<xprs::IVideoDecoder>&& videoDecoder)
      : xprsDecoder_{std::move(videoDecoder)} {}

  int decode(
      const vector<uint8_t>& encodedFrame,
      void* outDecodedFrame,
      const ImageContentBlockSpec& outputImageSpec) override {
    xprs::Frame frame;
    IF_ERROR_RETURN(xprsDecode(encodedFrame, frame));
    // xprs decodes to an internal buffer. We must copy/convert that pidel data to outDecodedFrame
    PixelFormat vrsPixelFormat = xprsToVrsPixelFormat(frame.fmt, outputImageSpec.getPixelFormat());
    if (vrsPixelFormat == PixelFormat::UNDEFINED) {
      XR_LOGE("Unsupported xprs pixel format: xprs::{}", getPixelFormatName(frame.fmt));
      return domainError(utils::DecodeStatus::UnsupportedPixelFormat);
    }
    if (vrsPixelFormat != outputImageSpec.getPixelFormat()) {
      XR_LOGE(
          "Unsupported XPRS to VRS pixel format conversion: xprs::{} to vrs::{}",
          getPixelFormatName(frame.fmt),
          outputImageSpec.getPixelFormatAsString());
      return domainError(utils::DecodeStatus::PixelFormatMismatch);
    }
    if (frame.width != outputImageSpec.getWidth() || frame.height != outputImageSpec.getHeight()) {
      XR_LOGE(
          "Unexpected dimensions {}x{}/{}x{}",
          outputImageSpec.getWidth(),
          outputImageSpec.getHeight(),
          frame.width,
          frame.height);
      return domainError(utils::DecodeStatus::UnexpectedImageDimensions);
    }
    xprsPixelFormat_ = frame.fmt;
    if (frame.fmt == xprs::PixelFormat::GBRP && vrsPixelFormat == PixelFormat::RGB8) {
      const uint32_t w = outputImageSpec.getWidth();
      const uint32_t h = outputImageSpec.getHeight();
      const uint8_t* gSrc = frame.planes[0];
      const uint8_t* bSrc = frame.planes[1];
      const uint8_t* rSrc = frame.planes[2];
      uint8_t* writeData = reinterpret_cast<uint8_t*>(outDecodedFrame);
      for (uint32_t line = 0; line < h; line++) {
        uint8_t* dest = writeData;
        for (uint32_t x = 0; x < w; x++) {
          *dest++ = rSrc[x];
          *dest++ = gSrc[x];
          *dest++ = bSrc[x];
        }
        gSrc += frame.stride[0];
        bSrc += frame.stride[1];
        rSrc += frame.stride[2];
        writeData += outputImageSpec.getStride();
      }
    } else if (frame.fmt == xprs::PixelFormat::YUV444P && vrsPixelFormat == PixelFormat::RGB8) {
      // xprs::PixelFormat::YUV444P has 3 distinct planes, Ocean's YUV24 has one with 3 channels...
      const uint32_t w = outputImageSpec.getWidth();
      const uint32_t h = outputImageSpec.getHeight();
      const uint8_t* ySrc = frame.planes[0];
      const uint8_t* uSrc = frame.planes[1];
      const uint8_t* vSrc = frame.planes[2];
      conversionBuffer_.resize(outputImageSpec.getRawImageSize());
      uint8_t* dest = conversionBuffer_.data();
      for (uint32_t line = 0; line < h; line++) {
        for (uint32_t x = 0; x < w; x++) {
          *dest++ = ySrc[x];
          *dest++ = uSrc[x];
          *dest++ = vSrc[x];
        }
        ySrc += frame.stride[0];
        uSrc += frame.stride[1];
        vSrc += frame.stride[2];
      }
      using namespace Ocean;
      FrameType sourceFrameType(w, h, Ocean::FrameType::FORMAT_YUV24, FrameType::ORIGIN_UPPER_LEFT);
      Frame sourceFrame(sourceFrameType, conversionBuffer_.data(), Frame::CM_USE_KEEP_LAYOUT);
      const FrameType targetFrameType(w, h, FrameType::FORMAT_RGB24, FrameType::ORIGIN_UPPER_LEFT);
      Frame targetFrame(targetFrameType, outDecodedFrame, Frame::CM_USE_KEEP_LAYOUT);
      XR_VERIFY(
          CV::FrameConverter::Comfort::convert(
              sourceFrame,
              targetFrameType.pixelFormat(),
              targetFrameType.pixelOrigin(),
              targetFrame,
              CV::FrameConverter::CP_ALWAYS_COPY,
              getWorkers(w * h)));
      XR_VERIFY(!targetFrame.isPlaneOwner()); // Beware of Ocean's backstabbing behaviors!
    } else if (frame.fmt == xprs::PixelFormat::YUV420P && vrsPixelFormat == PixelFormat::RGB8) {
      const uint32_t w = outputImageSpec.getWidth();
      const uint32_t h = outputImageSpec.getHeight();
      using namespace Ocean;
      FrameType sourceFrameType(
          w, h, Ocean::FrameType::FORMAT_Y_U_V12, FrameType::ORIGIN_UPPER_LEFT);
      Frame::PlaneInitializers<uint8_t> sourcePlaneInitializers = {
          {frame.planes[0], Frame::CM_USE_KEEP_LAYOUT, static_cast<uint32_t>(frame.stride[0] - w)},
          {frame.planes[1],
           Frame::CM_USE_KEEP_LAYOUT,
           static_cast<uint32_t>(frame.stride[1] - (w + 1) / 2)},
          {frame.planes[2],
           Frame::CM_USE_KEEP_LAYOUT,
           static_cast<uint32_t>(frame.stride[2] - (w + 1) / 2)},
      };
      Frame sourceFrame(sourceFrameType, sourcePlaneInitializers);
      const FrameType targetFrameType(w, h, FrameType::FORMAT_RGB24, FrameType::ORIGIN_UPPER_LEFT);
      Frame targetFrame(targetFrameType, outDecodedFrame, Frame::CM_USE_KEEP_LAYOUT);
      XR_VERIFY(
          CV::FrameConverter::Comfort::convert(
              sourceFrame,
              targetFrameType.pixelFormat(),
              targetFrameType.pixelOrigin(),
              targetFrame,
              CV::FrameConverter::CP_ALWAYS_COPY,
              getWorkers(w * h)));
      XR_VERIFY(!targetFrame.isPlaneOwner()); // Beware of Ocean's backstabbing behaviors!
    } else if (
        frame.fmt == xprs::PixelFormat::NV12 && vrsPixelFormat == PixelFormat::YUV_I420_SPLIT) {
      const uint32_t w = outputImageSpec.getWidth();
      const uint32_t h = outputImageSpec.getHeight();
      using namespace Ocean;
      FrameType sourceFrameType(
          w, h, Ocean::FrameType::FORMAT_Y_UV12, FrameType::ORIGIN_UPPER_LEFT); // NV12
      Frame::PlaneInitializers<uint8_t> srcPlaneInitializers = {
          {frame.planes[0],
           Frame::CM_USE_KEEP_LAYOUT,
           static_cast<uint32_t>(frame.stride[0] - frame.width)},
          {frame.planes[1],
           Frame::CM_USE_KEEP_LAYOUT,
           static_cast<uint32_t>(frame.stride[1] - frame.width)},
      };
      Frame sourceFrame(sourceFrameType, srcPlaneInitializers);
      const uint32_t yStride = outputImageSpec.getPlaneStride(0);
      const uint32_t uvStride = outputImageSpec.getPlaneStride(1);
      const uint32_t uvSize = uvStride * outputImageSpec.getPlaneHeight(1);
      const FrameType targetFrameType(
          w, h, FrameType::FORMAT_Y_U_V12, FrameType::ORIGIN_UPPER_LEFT); // I420
      uint8_t* yPlane = reinterpret_cast<uint8_t*>(outDecodedFrame);
      uint8_t* uPlane = yPlane + yStride * h;
      uint8_t* vPlane = uPlane + uvSize;
      Frame::PlaneInitializers<uint8_t> targetPlaneInitializers = {
          {yPlane, Frame::CM_USE_KEEP_LAYOUT, yStride - outputImageSpec.getDefaultStride()},
          {uPlane, Frame::CM_USE_KEEP_LAYOUT, uvStride - outputImageSpec.getDefaultStride2()},
          {vPlane, Frame::CM_USE_KEEP_LAYOUT, uvStride - outputImageSpec.getDefaultStride2()},
      };
      Frame targetFrame(targetFrameType, targetPlaneInitializers);
      XR_VERIFY(
          CV::FrameConverter::Comfort::convert(
              sourceFrame,
              targetFrameType.pixelFormat(),
              targetFrameType.pixelOrigin(),
              targetFrame,
              CV::FrameConverter::CP_ALWAYS_COPY,
              getWorkers(w * h)));
      if (targetFrame.isPlaneOwner()) {
        memcpy(yPlane, targetFrame.data<void>(0), w * h);
        memcpy(uPlane, targetFrame.data<void>(1), uvSize);
        memcpy(vPlane, targetFrame.data<void>(2), uvSize);
      }
    } else if (
        frame.fmt == xprs::PixelFormat::YUV420P && vrsPixelFormat == PixelFormat::YUV_420_NV12) {
      const uint32_t w = outputImageSpec.getWidth();
      const uint32_t w2 = (w + 1) / 2;
      const uint32_t h = outputImageSpec.getHeight();
      uint8_t* dst = reinterpret_cast<uint8_t*>(outDecodedFrame);
      Ocean::CV::FrameConverterY_U_V12::convertY_U_V12ToY_UV12(
          frame.planes[0],
          frame.planes[1],
          frame.planes[2],
          dst,
          dst + outputImageSpec.getPlaneHeight(0) * outputImageSpec.getPlaneStride(0),
          w,
          h,
          frame.stride[0] - w,
          frame.stride[1] - w2,
          frame.stride[2] - w2,
          outputImageSpec.getPlaneStride(0) - outputImageSpec.getDefaultStride(),
          outputImageSpec.getPlaneStride(1) - outputImageSpec.getDefaultStride2(),
          1,
          1,
          1,
          getWorkers(w * h));
    } else if (frame.fmt == xprs::PixelFormat::NV12 && vrsPixelFormat == PixelFormat::RGB8) {
      const uint32_t w = outputImageSpec.getWidth();
      const uint32_t h = outputImageSpec.getHeight();
      using namespace Ocean;
      FrameType sourceFrameType(
          w, h, Ocean::FrameType::FORMAT_Y_UV12, FrameType::ORIGIN_UPPER_LEFT); // NV12
      Frame::PlaneInitializers<uint8_t> srcPlaneInitializers = {
          {frame.planes[0],
           Frame::CM_USE_KEEP_LAYOUT,
           static_cast<uint32_t>(frame.stride[0] - frame.width)},
          {frame.planes[1],
           Frame::CM_USE_KEEP_LAYOUT,
           static_cast<uint32_t>(frame.stride[1] - frame.width)},
      };
      Frame sourceFrame(sourceFrameType, srcPlaneInitializers);
      const FrameType targetFrameType(w, h, FrameType::FORMAT_RGB24, FrameType::ORIGIN_UPPER_LEFT);
      Frame targetFrame(targetFrameType, outDecodedFrame, Frame::CM_USE_KEEP_LAYOUT);
      XR_VERIFY(
          CV::FrameConverter::Comfort::convert(
              sourceFrame,
              targetFrameType.pixelFormat(),
              targetFrameType.pixelOrigin(),
              targetFrame,
              CV::FrameConverter::CP_ALWAYS_COPY,
              getWorkers(w * h)));
      XR_VERIFY(!targetFrame.isPlaneOwner()); // Beware of Ocean's backstabbing behaviors!
    } else {
      uint32_t planeCount = outputImageSpec.getPlaneCount();
      uint8_t* dest = reinterpret_cast<uint8_t*>(outDecodedFrame);
      for (uint32_t plane = 0; plane < planeCount; plane++) {
        const uint8_t* src = frame.planes[plane];
        uint32_t dstHeight = outputImageSpec.getPlaneHeight(plane);
        uint32_t dstStride = outputImageSpec.getPlaneStride(plane);
        uint32_t copyStride = min<uint32_t>(dstStride, frame.stride[plane]);
        uint8_t* destLine = dest;
        uint32_t copyHeight = min<uint32_t>(dstHeight, frame.height);
        for (uint32_t height = 0; height < copyHeight; ++height) {
          memcpy(destLine, src, copyStride);
          destLine += dstStride;
          src += frame.stride[plane];
        }
        dest += dstHeight * dstStride;
      }
    }
    return SUCCESS;
  }
  int xprsDecode(const vector<uint8_t>& encodedFrame, xprs::Frame& frame) {
    xprs::Buffer compressedBuffer{};
    compressedBuffer.size = encodedFrame.size();
    compressedBuffer.data = const_cast<uint8_t*>(encodedFrame.data());
    XprsResult res = xprsDecoder_->decodeFrame(frame, compressedBuffer);
    if (res != XprsResult::OK) {
      XR_LOGE("Failed to decode xprs frame: {}", xprs::getErrorMessage(res));
      return domainError(utils::DecodeStatus::DecoderError);
    }
    return SUCCESS;
  }
  xprs::PixelFormat lastXprsPixelFormat() const {
    return xprsPixelFormat_;
  }

 protected:
  unique_ptr<xprs::IVideoDecoder> xprsDecoder_{};
  vector<uint8_t> conversionBuffer_{};
  xprs::PixelFormat xprsPixelFormat_{xprs::PixelFormat::UNKNOWN};
};
} // anonymous namespace

unique_ptr<utils::DecoderI> xprsDecoderMaker(
    const vector<uint8_t>& encodedFrame,
    void* outDecodedFrame,
    const ImageContentBlockSpec& outputImageSpec) {
  const string& codecFormatName = outputImageSpec.getCodecName();
  unique_ptr<VXDecoder> decoder;
  xprs::CodecList decoders;
  if (xprs::enumDecoders(decoders) != XprsResult::OK) {
    return nullptr;
  }
  xprs::VideoCodecFormat codecFormat{};
  if (getVideoCodecFormatFromName(codecFormat, codecFormatName) != XprsResult::OK) {
    return nullptr;
  }
  OperationContext context{"make_xprs_decoder", outputImageSpec.core().asString()};
  for (const auto& dec : decoders) {
    if (dec.format != codecFormat) {
      continue;
    }
    unique_ptr<xprs::IVideoDecoder> xprsDecoder{createDecoder(dec)};
    if (!xprsDecoder) {
      XR_LOGE("Creating xprs decoder '{}' for {} failed!", dec.implementationName, codecFormatName);
    } else {
      XprsResult res = xprsDecoder->init({});
      if (res != XprsResult::OK) {
        XR_LOGE(
            "Failed to initialized xprs decoder '{}' for {}: {}",
            dec.implementationName,
            codecFormatName,
            xprs::getErrorMessage(res));
        return nullptr;
      }
      decoder = make_unique<VXDecoder>(std::move(xprsDecoder));
      if (decoder->decode(encodedFrame, outDecodedFrame, outputImageSpec) == 0) {
        XR_LOGI(
            "Using {} {} decoder named '{}' (xprs::{} -> vrs::{})",
            codecFormatName,
            (dec.hwAccel ? "HW" : "SW"),
            dec.implementationName,
            getPixelFormatName(decoder->lastXprsPixelFormat()),
            outputImageSpec.getPixelFormatAsString());
        TelemetryLogger::info(
            context,
            codecFormatName + (dec.hwAccel ? "/HW/" : "/SW/") + dec.implementationName,
            getPixelFormatName(decoder->lastXprsPixelFormat()));
        return decoder;
      } else {
        XR_LOGW(
            "Failed to decode {} frame with {} decoder '{}'",
            codecFormatName,
            dec.hwAccel ? "HW" : "SW",
            dec.implementationName);
      }
    }
  }
  TelemetryLogger::error(context, "No decoder found for " + codecFormatName);
  return decoder;
}

} // namespace vrs::vxprs
