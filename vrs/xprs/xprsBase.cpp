// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

//
// See xprs.h for details.

#include "xprs.h"

#include <cstring>
#include <string_view>

namespace xprs {

constexpr std::string_view SUPPORTED_CODEC_FORMATS[] = {"H.264", "H.265", "VP9", "AV1"};

static_assert(
    sizeof(SUPPORTED_CODEC_FORMATS) / sizeof(SUPPORTED_CODEC_FORMATS[0]) ==
    int(VideoCodecFormat::LAST));

///
/// Verifies 'standard' is a valid enumerated video codec format and places the corresponding string
/// identifier in 'videoCodecStr'.
/// Returns 'XprsResult::OK' on success; 'XprsResult::ERR_INVALID_INPUT' on error.
///
XprsResult getNameFromVideoCodecFormat(std::string& videoCodecStr, VideoCodecFormat standard) {
  if (standard >= VideoCodecFormat::LAST) {
    return XprsResult::ERR_INVALID_INPUT;
  }

  videoCodecStr = SUPPORTED_CODEC_FORMATS[int(standard)];

  return XprsResult::OK;
}

const char* getNameFromVideoCodecFormat(VideoCodecFormat standard) {
  if (standard >= VideoCodecFormat::LAST) {
    return "Unknown";
  }

  return SUPPORTED_CODEC_FORMATS[int(standard)].data();
}

///
/// Verifies 'videoCodecStr' is a supported codec and places the corresponding enumerated
/// VideoCodecFormat in 'standard'.
/// Returns 'XprsResult::OK' on success; 'XprsResult::ERR_INVALID_INPUT' on error.
///
XprsResult getVideoCodecFormatFromName(
    VideoCodecFormat& standard,
    const VideoCodecName& videoCodecStr) {
  for (uint32_t s = static_cast<uint32_t>(VideoCodecFormat::FIRST);
       s < static_cast<uint32_t>(VideoCodecFormat::LAST);
       ++s) {
    if (videoCodecStr == SUPPORTED_CODEC_FORMATS[s]) {
      standard = static_cast<VideoCodecFormat>(s);
      return XprsResult::OK;
    }
  }

  return XprsResult::ERR_INVALID_INPUT;
}

const char* getPixelFormatName(PixelFormat pixelFormat) {
  switch (pixelFormat) {
    case PixelFormat::GRAY8:
      return "gray8";
    case PixelFormat::GRAY10LE:
      return "gray10le";
    case PixelFormat::GRAY12LE:
      return "gray12le";
    case PixelFormat::YUV420P:
      return "yuv420p";
    case PixelFormat::NV12:
      return "nv12";
    case PixelFormat::YUV420P10LE:
      return "yuv420p10le";
    case PixelFormat::YUV420P12LE:
      return "yuv420p12le";
    case PixelFormat::YUV422P:
      return "yuv422p";
    case PixelFormat::YUV444P:
      return "yuv444p";
    case PixelFormat::RGB24:
      return "rgb24";
    case PixelFormat::GBRP:
      return "gbrp";
    case PixelFormat::GBRP10LE:
      return "gbrp10le";
    case PixelFormat::GBRP12LE:
      return "gbrp12le";
    default:
      return "unknown";
  }
}

PixelFormat getPixelFormatFromName(const char* pixelFormatName) {
  if (strcmp(pixelFormatName, "gray8") == 0)
    return PixelFormat::GRAY8;
  else if (strcmp(pixelFormatName, "gray10le") == 0)
    return PixelFormat::GRAY10LE;
  else if (strcmp(pixelFormatName, "gray12le") == 0)
    return PixelFormat::GRAY12LE;
  else if (strcmp(pixelFormatName, "yuv420p") == 0)
    return PixelFormat::YUV420P;
  else if (strcmp(pixelFormatName, "nv12") == 0)
    return PixelFormat::NV12;
  else if (strcmp(pixelFormatName, "yuv420p10le") == 0)
    return PixelFormat::YUV420P10LE;
  else if (strcmp(pixelFormatName, "yuv420p12le") == 0)
    return PixelFormat::YUV420P12LE;
  else if (strcmp(pixelFormatName, "yuv422p") == 0)
    return PixelFormat::YUV422P;
  else if (strcmp(pixelFormatName, "yuv444p") == 0)
    return PixelFormat::YUV444P;
  else if (strcmp(pixelFormatName, "rgb24") == 0)
    return PixelFormat::RGB24;
  else if (strcmp(pixelFormatName, "gbrp") == 0)
    return PixelFormat::GBRP; ///< planer GBR 8 bit, 24bpp, GGG...G, BBB...B, RRR...R
  else if (strcmp(pixelFormatName, "gbrp10le") == 0)
    return PixelFormat::GBRP10LE; ///< planer GBR 10 bit, 30bpp, GGG...G, BBB...B, RRR...R
  else if (strcmp(pixelFormatName, "gbrp12le") == 0)
    return PixelFormat::GBRP12LE; ///< planer GBR 12 bit, 36bpp, GGG...G, BBB...B, RRR...R
  else
    return PixelFormat::UNKNOWN;
}

int getNumPlanes(xprs::PixelFormat pixelFmt) {
  switch (pixelFmt) {
    case PixelFormat::GRAY8:
    case PixelFormat::GRAY10LE:
    case PixelFormat::GRAY12LE:
    case PixelFormat::RGB24: // packed RGB
      return 1;
    case PixelFormat::NV12:
      return 2;
    case PixelFormat::YUV420P:
    case PixelFormat::YUV420P10LE:
    case PixelFormat::YUV420P12LE:
    case PixelFormat::YUV422P:
    case PixelFormat::YUV444P:
    case PixelFormat::GBRP:
    case PixelFormat::GBRP10LE:
    case PixelFormat::GBRP12LE:
      return 3;
    default:
      return 0;
  }
}

const char* getErrorMessage(XprsResult error) {
  switch (error) {
    case XprsResult::OK:
      return "Command completed succcessfully";
    case XprsResult::ERR_GENERIC:
      return "Generic error";
    case XprsResult::ERR_INVALID_CONFIG:
      return "Invalid configuration likely due to illegal combination of settings";
    case XprsResult::ERR_OOM:
      return "Out of memory";
    case XprsResult::ERR_NO_FRAME:
      return "Frame not available after encoding or decoding";
    case XprsResult::ERR_SYSTEM:
      return "System error";
    case XprsResult::ERR_FFMPEG:
      return "An error occurred in FFmpeg";
    case XprsResult::ERR_NOT_INITIALIZED:
      return "Enocder or decoder either not initizlized or failed during initialization";
    case XprsResult::ERR_INVALID_FRAME:
      return "Frame is misconfigured, corrupt or invalid";
    case XprsResult::ERR_CORRUPT_DATA:
      return "The decoder encountered corrupt input data";
    case XprsResult::ERR_INVALID_INPUT:
      return "An argument is invalid";
    case XprsResult::ERR_MUX_FAILURE:
      return "An error occurred in muxer";
    case XprsResult::ERR_NOT_IMPLEMENTED:
      return "Function not implemented";
    case XprsResult::ERR_UNKNOWN:
    default:
      break;
  }

  return "Unknown error";
}

} // namespace xprs
