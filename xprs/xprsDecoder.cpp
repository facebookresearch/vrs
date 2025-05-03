// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

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

#include <logging/Log.h>
#define DEFAULT_LOG_CHANNEL "XPRS"

namespace xprs {

CVideoDecoder::CVideoDecoder(const VideoCodec& codec) : _timeStamp{0} {
  *((VideoCodec*)this) = codec;
  _decoder = nullptr;
}

CVideoDecoder::~CVideoDecoder() {}

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
    case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_422YpCbCr8BiPlanarFullRange:
      return PixelFormat::YUV422P;
    case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_444YpCbCr8BiPlanarFullRange:
      return PixelFormat::YUV444P;
    default:
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
      pixelFormat != kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange &&
      pixelFormat != kCVPixelFormatType_422YpCbCr8BiPlanarFullRange &&
      pixelFormat != kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange &&
      pixelFormat != kCVPixelFormatType_444YpCbCr8BiPlanarFullRange) {
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
  if (pixelFormat == kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_422YpCbCr8BiPlanarFullRange) {
    // 422 doubles vertical chroma samples compared to 420
    frameSize = width * height * 2;
    uvHeight = height;
  } else if (
      pixelFormat == kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_444YpCbCr8BiPlanarFullRange) {
    // 444 doubles both vertical and horizontal chroma samples compared to 420
    frameSize = width * height * 3;
    uvHeight = height;
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
    if (pixelFormat == kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange ||
        pixelFormat == kCVPixelFormatType_422YpCbCr8BiPlanarFullRange) {
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

    } else if (
        pixelFormat == kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange ||
        pixelFormat == kCVPixelFormatType_444YpCbCr8BiPlanarFullRange) {
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
