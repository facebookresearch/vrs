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

#include "Codecs.h"
#include "NvCodecConfig.h"
#include "xprsDecoder.h"
#include "xprsUtils.h"

#include <algorithm>
#include <cstdlib>
#include <string_view>

#ifdef WITH_DAV1D
#include "Dav1dDecode.h"
#endif

#define DEFAULT_LOG_CHANNEL "XPRS"
#include <logging/Log.h>

namespace xprs {

static const std::string_view kPreferredDecoderImplementations[] = {
    kH265DecoderName,
    kH264DecoderName,
#ifdef XPRS_HAS_NVDEC
    kNvH264DecoderName,
    kNvH265DecoderName,
    kNvAv1DecoderName,
#endif
#if defined(HAS_VP9) && HAS_VP9 == 1
    kVp9DecoderName,
#endif
#ifdef WITH_DAV1D
    // Prefer dav1d over libaom for AV1 whenever it is linked.
    // Builds without WITH_DAV1D fall back to libaom.
    kDav1dDecoderName,
#endif
    kAomDecoderName,
};

namespace {

bool findDecoderByName(const std::string_view& name, VideoCodec& codec) {
#ifdef WITH_DAV1D
  if (name == kDav1dDecoderName) {
    codec = VideoCodec{VideoCodecFormat::AV1, kDav1dDecoderName.data(), false};
    return true;
  }
#endif

  // Check ffmpeg first
  const AVCodec* avCodec = avcodec_find_decoder_by_name(name.data());
  if (avCodec != nullptr) {
    codec = VideoCodec{mapToVideoCodecFormat(avCodec->id), avCodec->name, isHardwareCodec(avCodec)};
    return true;
  }

  // Add custom decoders
#ifdef XPRS_HAS_NVDEC
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

// Set XPRS_DISABLE_HW_DECODE to any value to force CPU-only decoding. Useful
// for deterministic results, working around GPU memory pressure, or comparing
// HW-vs-SW output. Read once at first call so the value is fixed for the
// process lifetime — runtime mutation of the env var has no effect.
bool isHwDecodeDisabled() {
  static const bool disabled = std::getenv("XPRS_DISABLE_HW_DECODE") != nullptr;
  return disabled;
}

} // namespace

///
/// Enumerate all available decoders.
///
XprsResult enumDecoders(CodecList& codecs, bool hwCapabilityCheck) {
  XprsResult result = XprsResult::OK;

  codecs.clear();
  codecs.reserve(std::size(kPreferredDecoderImplementations));
  const bool hwDisabled = isHwDecodeDisabled();
  try {
    for (const auto& impl : kPreferredDecoderImplementations) {
      VideoCodec codec;
      if (findDecoderByName(impl, codec)) {
        if (codec.hwAccel && hwDisabled) {
          XR_LOGI(
              "Skipping HW decoder {} (XPRS_DISABLE_HW_DECODE is set)", codec.implementationName);
          continue;
        }
        if (codec.hwAccel && hwCapabilityCheck) {
#ifdef XPRS_HAS_NVDEC
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
    // Downgraded from XR_LOGE: on non-GPU machines this fires every time a VRS
    // file is opened (CUDA init throws). Callers fall back to SW decoders that
    // were already collected before the throw, so this is expected, not an error.
    XR_LOGW("HW decoder enumeration skipped: {}", convertExceptionToError(e, result));
  }

  // stable_sort so decoders with equal hwAccel keep their
  // kPreferredDecoderImplementations order (e.g. dav1d ahead of libaom for AV1).
  std::stable_sort(codecs.begin(), codecs.end(), [](const VideoCodec& lhs, const VideoCodec& rhs) {
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
  codecs.reserve(std::size(kPreferredDecoderImplementations));
  const bool hwDisabled = isHwDecodeDisabled();
  try {
    for (const auto& impl : kPreferredDecoderImplementations) {
      VideoCodec codec;
      if (findDecoderByName(impl, codec)) {
        if (codec.format == standard) {
          if (codec.hwAccel && hwDisabled) {
            XR_LOGI(
                "Skipping HW decoder {} (XPRS_DISABLE_HW_DECODE is set)", codec.implementationName);
            continue;
          }
          if (codec.hwAccel && hwCapabilityCheck) {
#ifdef XPRS_HAS_NVDEC
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
    XR_LOGW("HW decoder enumeration skipped: {}", convertExceptionToError(e, result));
  }

  // stable_sort so decoders with equal hwAccel keep their
  // kPreferredDecoderImplementations order (e.g. dav1d ahead of libaom for AV1).
  std::stable_sort(codecs.begin(), codecs.end(), [](const VideoCodec& lhs, const VideoCodec& rhs) {
    return lhs.hwAccel > rhs.hwAccel;
  });

  return result;
}

IVideoDecoder* createDecoder(const VideoCodec& codec) {
#ifdef WITH_DAV1D
  if (codec.implementationName == kDav1dDecoderName) {
    return new (std::nothrow) Dav1dVideoDecoder();
  }
#endif
  IVideoDecoder* result = new (std::nothrow) CVideoDecoder(codec);
  return result;
}

bool hasCudaSupport() noexcept {
#ifdef XPRS_HAS_NVDEC
  // Compile-time NVDEC is wired in. Runtime check: try to bring up the
  // CUDA context once. If the driver is missing, the kernel module isn't
  // loaded, or no NVIDIA device is visible, getNvCodecContext() throws and
  // we report no support. NvCodecContextProvider caches the result via
  // its tri-state atomic (see cudaContextProvider.cpp), so subsequent
  // calls are a single atomic load.
  try {
    NvCodecContextProvider::getNvCodecContext();
    return true;
  } catch (...) {
    return false;
  }
#else
  return false;
#endif
}

} // namespace xprs
