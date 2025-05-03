// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <string_view>

#ifdef WITH_NVCODEC
#include "nvEncoder.h"
#endif

namespace xprs {

// Encoder implementations
constexpr std::string_view kx265EncoderName = "libx265";
constexpr std::string_view kx264EncoderName = "libx264";
constexpr std::string_view kNvH265EncoderName = "hevc_nvenc";
constexpr std::string_view kNvH264EncoderName = "h264_nvenc";
constexpr std::string_view kNvAv1XprsWrapEncoderName = "av1_nvenc";
constexpr std::string_view kVp9EncoderName = "libvpx-vp9";
constexpr std::string_view kH264RgbEncoderName = "libx264rgb";
constexpr std::string_view kSvtAv1EncoderName = "libsvtav1";

// Decoder implementations
constexpr std::string_view kH265DecoderName = "hevc";
constexpr std::string_view kH264DecoderName = "h264";
constexpr std::string_view kNvH264DecoderName = "h264_cuvid";
constexpr std::string_view kNvH265DecoderName = "hevc_cuvid";
constexpr std::string_view kNvAv1DecoderName = "av1_cuvid";
constexpr std::string_view kVp9DecoderName = "vp9";
constexpr std::string_view kAomDecoderName = "libaom-av1";

} // namespace xprs
