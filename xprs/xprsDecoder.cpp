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

//
// CVideoDecoder class is an implementation of the IVideoDecoder interface class.
// See xprsDecoder.h for details.

#include "xprsDecoder.h"
#ifdef WITH_NVCODEC
#include "cudaContextProvider.h"
#endif
#ifdef WITH_NVCODEC
#include "Codecs.h"
#endif
#include "xprsUtils.h"

#ifdef __APPLE__
#include <CoreVideo/CoreVideo.h>
#include <libavutil/hwcontext_videotoolbox.h>
#endif

#define DEFAULT_LOG_CHANNEL "XPRS"
#include <logging/Log.h>

namespace xprs {

CVideoDecoder::CVideoDecoder(const VideoCodec& codec) : _timeStamp{0} {
  *((VideoCodec*)this) = codec;
  _decoder = nullptr;
}

CVideoDecoder::~CVideoDecoder() = default;

XprsResult CVideoDecoder::init(bool disableHwAcceleration) {
  XprsResult result = XprsResult::OK;
  try {
#ifdef WITH_NVCODEC
    if (implementationName == kNvH264DecoderName || implementationName == kNvH265DecoderName ||
        implementationName == kNvAv1DecoderName) {
      const NvCodecContext nvcodec_context = NvCodecContextProvider::getNvCodecContext();
      const cudaVideoCodec cuda_video_codec = codecNameToCudaVideoCodecEnum(implementationName);
      _decoder.reset(new NvDecoder(nvcodec_context, cuda_video_codec));
    } else
#endif
    {
      _decoder = std::make_unique<VideoDecode>(implementationName.c_str(), disableHwAcceleration);
      _expectHwFrame = _decoder->isHwAccelerated();
    }
    _decoder->open();
  } catch (std::exception& e) {
    XR_LOGE("{}", convertExceptionToError(e, result));
    _decoder.reset();
  }

  return result;
}

#ifdef __APPLE__

// kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange and
// kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange were introduced in macOS 15.0 / iOS 18.0 SDK
#if (defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000) ||   \
    (defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 180000) || \
    (defined(__TV_OS_VERSION_MAX_ALLOWED) && __TV_OS_VERSION_MAX_ALLOWED >= 180000) ||         \
    (defined(__WATCH_OS_VERSION_MAX_ALLOWED) && __WATCH_OS_VERSION_MAX_ALLOWED >= 110000)
#define XPRS_HAS_422_BIPLANAR_FORMATS() 1
#define XPRS_HAS_444_BIPLANAR_FORMATS() 1
#else
#define XPRS_HAS_422_BIPLANAR_FORMATS() 0
#define XPRS_HAS_444_BIPLANAR_FORMATS() 0
#endif

static bool is422BiplanarFormat(OSType pixelFormat) {
#if XPRS_HAS_422_BIPLANAR_FORMATS()
  return pixelFormat == kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_422YpCbCr8BiPlanarFullRange;
#else
  return false;
#endif
}

static bool is444BiplanarFormat(OSType pixelFormat) {
#if XPRS_HAS_444_BIPLANAR_FORMATS()
  return pixelFormat == kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_444YpCbCr8BiPlanarFullRange;
#else
  return false;
#endif
}

static PixelFormat convertVideoToolboxPixelFormat(OSType videotoolboxFormat) {
  switch (videotoolboxFormat) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
      return PixelFormat::NV12;
    case kCVPixelFormatType_420YpCbCr8Planar:
      return PixelFormat::YUV420P;
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
      return PixelFormat::NV1210LE;
    default:
      if (is422BiplanarFormat(videotoolboxFormat)) {
        return PixelFormat::YUV422P;
      }
      if (is444BiplanarFormat(videotoolboxFormat)) {
        return PixelFormat::YUV444P;
      }
      return PixelFormat::UNKNOWN;
  }
}

#endif

void CVideoDecoder::convertAVFrame(const AVFrame* avframe, Frame& frameOut) {
  if (avframe->format != AV_PIX_FMT_VIDEOTOOLBOX) {
    if (_expectHwFrame) {
      XR_LOGI("Fallback to software decoding, likely due to unsupported color format");
      _expectHwFrame = false;
    }
    convertAVFrameToFrame(avframe, frameOut);
    return;
  }

#ifdef __APPLE__
  // Access the frame data through the CVPixelBufferRef
  CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)_pix.avFrame()->data[3];
  if (!pixelBuffer) {
    return;
  }
  OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

  if (pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange &&
      pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange &&
      pixelFormat != kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange &&
      pixelFormat != kCVPixelFormatType_420YpCbCr10BiPlanarFullRange &&
      !is422BiplanarFormat(pixelFormat) && !is444BiplanarFormat(pixelFormat)) {
    return;
  }

  size_t width = CVPixelBufferGetWidth(pixelBuffer);
  size_t height = CVPixelBufferGetHeight(pixelBuffer);
  size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  size_t uvStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

  size_t bytes = 1;
  if (pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange) {
    bytes = 2;
  }

  // Default to 420 format
  size_t uvHeight = height / 2;
  size_t frameSize = width * height * 3 / 2;
  if (is422BiplanarFormat(pixelFormat)) {
    // 422 doubles vertical chroma samples compared to 420
    frameSize = width * height * 2;
    uvHeight = height;
  } else if (is444BiplanarFormat(pixelFormat)) {
    // 444 doubles both vertical and horizontal chroma samples compared to 420
    frameSize = width * height * 3;
    uvHeight = height;
  } else {
    // 420 format - use default values above
  }
  frameSize *= bytes;
  if (_buffer.size() < frameSize) {
    _buffer.resize(frameSize);
  }

  if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess) {
    // Copy Y plane
    uint8_t* yPlane = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
    for (size_t y = 0; y < height; y++) {
      memcpy(_buffer.data() + y * width * bytes, yPlane + y * yStride, width * bytes);
    }

    // Convert biplanar 422 and 444 to triplanar
    if (is422BiplanarFormat(pixelFormat)) {
      uint8_t* uvPlane = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
      uint8_t* uPlane = _buffer.data() + width * height;
      uint8_t* vPlane = uPlane + (width * uvHeight / 2);

      for (size_t y = 0; y < uvHeight; y++) {
        for (size_t x = 0; x < width; x += 2) {
          uPlane[y * (width / 2) + x / 2] = uvPlane[y * uvStride + x];
          vPlane[y * (width / 2) + x / 2] = uvPlane[y * uvStride + x + 1];
        }
      }
      frameOut.numPlanes = 3;

      frameOut.planes[1] = uPlane;
      frameOut.stride[1] = width / 2;
      frameOut.planes[2] = vPlane;
      frameOut.stride[2] = width / 2;

    } else if (is444BiplanarFormat(pixelFormat)) {
      uint8_t* uvPlane = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
      uint8_t* uPlane = _buffer.data() + width * height;
      uint8_t* vPlane = uPlane + (width * height);

      for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width * 2; x += 2) {
          uPlane[y * width + x / 2] = uvPlane[y * uvStride + x];
          vPlane[y * width + x / 2] = uvPlane[y * uvStride + x + 1];
        }
      }
      frameOut.numPlanes = 3;
      frameOut.planes[1] = uPlane;
      frameOut.stride[1] = width;
      frameOut.planes[2] = vPlane;
      frameOut.stride[2] = width;
    } else {
      // Copy the UV plane for NV12
      uint8_t* uvPlane = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
      for (size_t y = 0; y < uvHeight; y++) {
        memcpy(
            _buffer.data() + width * height * bytes + y * width * bytes,
            uvPlane + y * uvStride,
            width * bytes);
      }
      frameOut.numPlanes = 2;
      frameOut.planes[1] = _buffer.data() + width * height * bytes;
      frameOut.stride[1] = width * bytes;
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
  }

  if (pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange) {
    // videotoolbox stores 10 bit data in the MSB of each 16bits, we need to shift it to LSB
    for (size_t i = 0; i < frameSize; i += 2) {
      uint16_t* p = (uint16_t*)(_buffer.data() + i);
      *p = (*p >> 6);
    }
  }

  frameOut.width = width;
  frameOut.height = height;
  frameOut.planes[0] = _buffer.data();
  frameOut.stride[0] = width * bytes;
  frameOut.fmt = convertVideoToolboxPixelFormat(pixelFormat);
  frameOut.ptsMs = _pix.avFrame()->pts;
  frameOut.keyFrame = (_pix.avFrame()->pict_type == AV_PICTURE_TYPE_I);

#endif
}

XprsResult CVideoDecoder::decodeFrame(Frame& frameOut, const Buffer& compressed) {
  if (!_decoder) {
    return XprsResult::ERR_NOT_INITIALIZED;
  }

  XprsResult result = XprsResult::OK;

  try {
    _decoder->decode(compressed.data, compressed.size, _pix);
    if (_pix.avFrame()->flags & AV_FRAME_FLAG_CORRUPT) {
      result = XprsResult::ERR_CORRUPT_DATA;
    } else if (_pix.avFrame()->flags & AV_FRAME_FLAG_DISCARD) {
      result = XprsResult::ERR_NO_FRAME;
    } else {
      // we create an artificial pts because we are dealing with raw bitstreams, which don't have
      // this information
      _pix.avFrame()->pts = _timeStamp++;
      convertAVFrame(_pix.avFrame(), frameOut);
    }
  } catch (std::exception& e) {
    XR_LOGE("{}", convertExceptionToError(e, result));
  }

  return result;
}

} // namespace xprs
