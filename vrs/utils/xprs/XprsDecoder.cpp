// Facebook Technologies, LLC Proprietary and Confidential.

#include "XprsDecoder.h"

#include <algorithm>

#include <xprs.h>

#if USE_OCEAN
#include <ocean/base/Frame.h>
#include <ocean/cv/FrameConverter.h>
#endif

#define DEFAULT_LOG_CHANNEL "XprsDecoder"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/ErrorCode.h>
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
      // some codecs don't support GREY8, and will silently convert to YUV420
      if (vrsPixelFormat == PixelFormat::GREY8) {
        return PixelFormat::GREY8;
      }
      return PixelFormat::YUV_I420_SPLIT;
    case xprs::PixelFormat::YUV444P:
      return PixelFormat::RGB8; // Using Ocean to convert!
    case xprs::PixelFormat::YUV420P10LE:
      // some codecs don't support GREY10, and will silently convert to YUV420P10LE
      if (vrsPixelFormat == PixelFormat::GREY10) {
        return PixelFormat::GREY10;
      }
    default:
      break;
  }
  return PixelFormat::UNDEFINED;
}

} // namespace

namespace vrs::vxprs {

using namespace std;
using xprs::XprsResult;

class Decoder : public vrs::utils::DecoderI {
 public:
  Decoder(unique_ptr<xprs::IVideoDecoder>&& xprsDecoder) : xprsDecoder_{move(xprsDecoder)} {}

  int decode(
      RecordReader* reader,
      const uint32_t sizeBytes,
      void* outBuffer,
      const ImageContentBlockSpec& expectedSpec) override {
    xprs::Frame frame;
    IF_ERROR_RETURN(decode(reader, sizeBytes, frame));
    PixelFormat vrsPixelFormat = xprsToVrsPixelFormat(frame.fmt, expectedSpec.getPixelFormat());
    if (vrsPixelFormat == PixelFormat::UNDEFINED) {
      XR_LOGE("Unsupported xprs pixel format: {}", frame.fmt);
      return domainError(utils::DecodeStatus::UnsupportedPixelFormat);
    }
    if (vrsPixelFormat != expectedSpec.getPixelFormat()) {
      XR_LOGE(
          "VRS / XPRS pixel format mismatch: {}/{}",
          expectedSpec.getPixelFormatAsString(),
          frame.fmt);
      return domainError(utils::DecodeStatus::PixelFormatMismatch);
    }
    if (frame.width != expectedSpec.getWidth() || frame.height != expectedSpec.getHeight()) {
      XR_LOGE(
          "Unexpected dimensions {}x{}/{}x{}",
          expectedSpec.getWidth(),
          expectedSpec.getHeight(),
          frame.width,
          frame.height);
      return domainError(utils::DecodeStatus::UnexpectedImageDimensions);
    }
    if (xprsPixelFormat_ != frame.fmt) {
      xprsPixelFormat_ = frame.fmt;
      XR_LOGI(
          "Decoding frames from {} to {}.",
          getPixelFormatName(frame.fmt),
          expectedSpec.getPixelFormatAsString());
    }
    if (frame.fmt == xprs::PixelFormat::GBRP && vrsPixelFormat == PixelFormat::RGB8) {
      const uint32_t w = expectedSpec.getWidth();
      const uint32_t h = expectedSpec.getHeight();
      const uint8_t* gSrc = frame.planes[0];
      const uint8_t* bSrc = frame.planes[1];
      const uint8_t* rSrc = frame.planes[2];
      uint8_t* writeData = reinterpret_cast<uint8_t*>(outBuffer);
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
        writeData += expectedSpec.getStride();
      }
    } else if (frame.fmt == xprs::PixelFormat::YUV444P && vrsPixelFormat == PixelFormat::RGB8) {
      // xprs::PixelFormat::YUV444P has 3 distinct planes, Ocean's YUV24 has one with 3 channels...
      const uint32_t w = expectedSpec.getWidth();
      const uint32_t h = expectedSpec.getHeight();
      const uint8_t* ySrc = frame.planes[0];
      const uint8_t* uSrc = frame.planes[1];
      const uint8_t* vSrc = frame.planes[2];
      conversionBuffer_.resize(expectedSpec.getRawImageSize());
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
#if USE_OCEAN
      using namespace Ocean;
      FrameType sourceFrameType(w, h, Ocean::FrameType::FORMAT_YUV24, FrameType::ORIGIN_UPPER_LEFT);
      Frame sourceFrame(sourceFrameType, conversionBuffer_.data(), Frame::CM_USE_KEEP_LAYOUT);
      const FrameType targetFrameType(w, h, FrameType::FORMAT_RGB24, FrameType::ORIGIN_UPPER_LEFT);
      Frame targetFrame(targetFrameType, outBuffer, Frame::CM_USE_KEEP_LAYOUT);
      XR_VERIFY(CV::FrameConverter::Comfort::convert(
          sourceFrame, targetFrameType, targetFrame, CV::FrameConverter::CP_ALWAYS_COPY));
      XR_VERIFY(!targetFrame.isPlaneOwner()); // Beware of Ocean's backstabbing behaviors!
#endif
    } else {
      uint32_t planeCount = expectedSpec.getPlaneCount();
      uint8_t* dest = reinterpret_cast<uint8_t*>(outBuffer);
      for (uint32_t plane = 0; plane < planeCount; plane++) {
        const uint8_t* src = frame.planes[plane];
        uint32_t dstHeight = expectedSpec.getPlaneHeight(plane);
        uint32_t dstStride = expectedSpec.getPlaneStride(plane);
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
  int decode(RecordReader* reader, const uint32_t sizeBytes) override {
    xprs::Frame frame;
    return decode(reader, sizeBytes, frame);
  }
  int decode(RecordReader* reader, const uint32_t sizeBytes, xprs::Frame& frame) {
    buffer_.resize(sizeBytes);
    int error = reader->read(buffer_.data(), sizeBytes);
    if (error != 0) {
      XR_LOGE("Failed to read encoded frame: {}", errorCodeToMessage(error));
      return error;
    }
    xprs::Buffer compressedBuffer;
    compressedBuffer.size = sizeBytes;
    compressedBuffer.data = buffer_.data();
    XprsResult res = xprsDecoder_->decodeFrame(frame, compressedBuffer);
    if (res != XprsResult::OK) {
      XR_LOGE("Failed to decode xprs frame: {}", res);
      return domainError(utils::DecodeStatus::DecoderError);
    }
    return SUCCESS;
  }

 protected:
  unique_ptr<xprs::IVideoDecoder> xprsDecoder_;
  vector<uint8_t> buffer_;
  vector<uint8_t> conversionBuffer_;
  xprs::PixelFormat xprsPixelFormat_{xprs::PixelFormat::UNKNOWN};
};

unique_ptr<utils::DecoderI> xprsDecoderMaker(const string& codecFormatName) {
  unique_ptr<utils::DecoderI> decoder;
  xprs::CodecList decoders;
  if (xprs::enumDecoders(decoders) != XprsResult::OK) {
    return nullptr;
  }
  xprs::VideoCodecFormat codecFormat;
  if (getVideoCodecFormatFromName(codecFormat, codecFormatName) != XprsResult::OK) {
    return nullptr;
  }
  for (const auto& dec : decoders) {
    if (dec.format == codecFormat) {
      unique_ptr<xprs::IVideoDecoder> xprsDecoder{createDecoder(dec)};
      if (!xprsDecoder) {
        XR_LOGE(
            "Creating xprs decoder '{}' for {} failed!", dec.implementationName, codecFormatName);
      } else {
        XprsResult res = xprsDecoder->init({});
        if (res != XprsResult::OK) {
          XR_LOGE(
              "Failed to initialized xprs decoder '{}' for {}: {}",
              dec.implementationName,
              codecFormatName,
              res);
          return nullptr;
        }
        decoder = make_unique<Decoder>(move(xprsDecoder));
        XR_LOGI("Using decoder named '{}' for {}", dec.implementationName, codecFormatName);
        break;
      }
    }
  }
  return decoder;
}

} // namespace vrs::vxprs
