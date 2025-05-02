// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "Codecs.h"
#include "xprsDecoder.h"
#include "xprsUtils.h"

#include <algorithm>
#include <string_view>

#define DEFAULT_LOG_CHANNEL "XPRS"
#include <logging/Log.h>

namespace xprs {

static const std::string_view kPreferredDecoderImplentations[] = {
    kH265DecoderName,
    kH264DecoderName,
#ifdef WITH_NVCODEC
    kNvH264DecoderName,
    kNvH265DecoderName,
    kNvAv1DecoderName,
#endif
#if defined(HAS_VP9) && HAS_VP9 == 1
    kVp9DecoderName,
#endif
    kAomDecoderName,
};

namespace {

bool findDecoderByName(const std::string_view& name, VideoCodec& codec) {
  // Check ffmpeg first
  const AVCodec* avCodec = avcodec_find_decoder_by_name(name.data());
  if (avCodec != nullptr) {
    codec = VideoCodec{mapToVideoCodecFormat(avCodec->id), avCodec->name, isHardwareCodec(avCodec)};
    return true;
  }

  // Add custom decoders
#ifdef WITH_NVCODEC
  if (name == kNvH265DecoderName) {
    codec = VideoCodec{VideoCodecFormat::H265, name.data(), true};
    return true;
  }
  if (name == kNvH264DecoderName) {
    codec = VideoCodec{VideoCodecFormat::H264, name.data(), true};
    return true;
  }
  if (name == kNvAv1DecoderName) {
    codec = VideoCodec{VideoCodecFormat::AV1, name.data(), true};
    return true;
  }
#endif

  return false;
}

} // namespace

///
/// Enumerate all available decoders.
///
XprsResult enumDecoders(CodecList& codecs, bool hwCapabilityCheck) {
  XprsResult result = XprsResult::OK;

  codecs.clear();
  codecs.reserve(std::size(kPreferredDecoderImplentations));
  try {
    for (const auto& impl : kPreferredDecoderImplentations) {
      VideoCodec codec;
      if (findDecoderByName(impl, codec)) {
        if (codec.hwAccel && hwCapabilityCheck) {
#ifdef WITH_NVCODEC
          const NvCodecContext nvcodecContext = NvCodecContextProvider::getNvCodecContext();
          if (deviceHasNoHwDecoder(codec.implementationName, nvcodecContext._device_name)) {
            XR_LOGI(
                "Skipping HW decoder {}  because detected device {} does not support it.",
                codec.implementationName,
                nvcodecContext._device_name);
            continue;
          }
#endif
        }
        codecs.push_back(codec);
      }
    }
  } catch (std::exception& e) {
    XR_LOGE("{}", convertExceptionToError(e, result));
  }

  std::sort(codecs.begin(), codecs.end(), [](const VideoCodec& lhs, const VideoCodec& rhs) {
    return lhs.hwAccel > rhs.hwAccel;
  });

  return result;
}

///
/// Enumerate all available decoders for a give codec. In the beginning before we
/// have HW support, this would return just 1 codec.
///
XprsResult
enumDecodersByFormat(CodecList& codecs, VideoCodecFormat standard, bool hwCapabilityCheck) {
  XprsResult result = XprsResult::OK;

  codecs.clear();
  codecs.reserve(std::size(kPreferredDecoderImplentations));
  try {
    for (const auto& impl : kPreferredDecoderImplentations) {
      VideoCodec codec;
      if (findDecoderByName(impl, codec)) {
        if (codec.format == standard) {
          if (codec.hwAccel && hwCapabilityCheck) {
#ifdef WITH_NVCODEC
            const NvCodecContext nvcodecContext = NvCodecContextProvider::getNvCodecContext();
            if (deviceHasNoHwDecoder(codec.implementationName, nvcodecContext._device_name)) {
              XR_LOGI(
                  "Skipping HW decoder {}  because detected device {} does not support it.",
                  codec.implementationName,
                  nvcodecContext._device_name);
              continue;
            }
#endif
          }
          codecs.push_back(codec);
        }
      }
    }
  } catch (std::exception& e) {
    XR_LOGE("{}", convertExceptionToError(e, result));
  }

  std::sort(codecs.begin(), codecs.end(), [](const VideoCodec& lhs, const VideoCodec& rhs) {
    return lhs.hwAccel > rhs.hwAccel;
  });

  return result;
}

IVideoDecoder* createDecoder(const VideoCodec& codec) {
  IVideoDecoder* result = new (std::nothrow) CVideoDecoder(codec);
  return result;
}

} // namespace xprs
