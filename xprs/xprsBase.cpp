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
// See xprs.h for details.

#include "xprs.h"

#include <cstring>
#include <string_view>
#include <unordered_map>

namespace xprs {

constexpr std::string_view SUPPORTED_CODEC_FORMATS[] = {"H.264", "H.265", "VP9", "AV1"};

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
  constexpr std::pair<PixelFormat, const char*> pixelFormatArray[] = {
      {PixelFormat::GRAY8, "gray8"},
      {PixelFormat::GRAY10LE, "gray10le"},
      {PixelFormat::GRAY12LE, "gray12le"},
      {PixelFormat::YUV420P, "yuv420p"},
      {PixelFormat::NV12, "nv12"},
      {PixelFormat::YUV420P10LE, "yuv420p10le"},
      {PixelFormat::YUV420P12LE, "yuv420p12le"},
      {PixelFormat::YUV422P, "yuv422p"},
      {PixelFormat::YUV444P, "yuv444p"},
      {PixelFormat::RGB24, "rgb24"},
      {PixelFormat::GBRP, "gbrp"},
      {PixelFormat::GBRP10LE, "gbrp10le"},
      {PixelFormat::GBRP12LE, "gbrp12le"},
      {PixelFormat::NV1210LE, "nv1210le"},
      {PixelFormat::NV1212LE, "nv1212le"}};

  static_assert(
      sizeof(pixelFormatArray) / sizeof(pixelFormatArray[0]) == int(PixelFormat::COUNT) - 1,
      "Array size does not match enum size");

  static const std::unordered_map<PixelFormat, const char*> pixelFormatMap(
      std::begin(pixelFormatArray), std::end(pixelFormatArray));

  auto it = pixelFormatMap.find(pixelFormat);
  if (it != pixelFormatMap.end()) {
    return it->second;
  } else {
    return "unknown";
  }
}

PixelFormat getPixelFormatFromName(const char* pixelFormatName) {
  constexpr std::pair<const char*, PixelFormat> pixelFormatArray[] = {
      {"gray8", PixelFormat::GRAY8},
      {"gray10le", PixelFormat::GRAY10LE},
      {"gray12le", PixelFormat::GRAY12LE},
      {"yuv420p", PixelFormat::YUV420P},
      {"nv12", PixelFormat::NV12},
      {"yuv420p10le", PixelFormat::YUV420P10LE},
      {"yuv420p12le", PixelFormat::YUV420P12LE},
      {"yuv422p", PixelFormat::YUV422P},
      {"yuv444p", PixelFormat::YUV444P},
      {"rgb24", PixelFormat::RGB24},
      {"gbrp", PixelFormat::GBRP},
      {"gbrp10le", PixelFormat::GBRP10LE},
      {"gbrp12le", PixelFormat::GBRP12LE},
      {"nv1210le", PixelFormat::NV1210LE},
      {"nv1212le", PixelFormat::NV1212LE}};

  static_assert(
      sizeof(pixelFormatArray) / sizeof(pixelFormatArray[0]) == int(PixelFormat::COUNT) - 1,
      "Array size does not match enum size");

  static const std::unordered_map<const char*, PixelFormat> pixelFormatMap(
      std::begin(pixelFormatArray), std::end(pixelFormatArray));

  if (pixelFormatName == nullptr) {
    return PixelFormat::UNKNOWN;
  }

  auto it = pixelFormatMap.find(pixelFormatName);
  if (it != pixelFormatMap.end()) {
    return it->second;
  } else {
    return PixelFormat::UNKNOWN;
  }
}

int getNumPlanes(xprs::PixelFormat pixelFmt) {
  switch (pixelFmt) {
    case PixelFormat::GRAY8:
    case PixelFormat::GRAY10LE:
    case PixelFormat::GRAY12LE:
    case PixelFormat::RGB24: // packed RGB
      return 1;
    case PixelFormat::NV12:
    case PixelFormat::NV1210LE:
    case PixelFormat::NV1212LE:
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
