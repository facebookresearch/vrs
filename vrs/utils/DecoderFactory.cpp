// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include "DecoderFactory.h"

#include <map>
#include <mutex>

#define DEFAULT_LOG_CHANNEL "DecoderFactory"
#include <logging/Log.h>

#include <vrs/ErrorCode.h>

using namespace std;

namespace vrs {

template <>
ErrorDomain getErrorDomain<utils::DecodeStatus>() {
  static ErrorDomain sGaiaErrorDomain = newErrorDomain("Decoder");
  return sGaiaErrorDomain;
}

template <>
const std::map<utils::DecodeStatus, const char*>& getErrorCodeRegistry<utils::DecodeStatus>() {
  static map<utils::DecodeStatus, const char*> sRegistry;
  static once_flag sSingleInitFlag;
  call_once(sSingleInitFlag, [] {
    sRegistry = {
        {utils::DecodeStatus::DecoderError, "Codec error."},
        {utils::DecodeStatus::CodecNotFound, "Video codec not found."},
        {utils::DecodeStatus::FrameSequenceError, "Video frame out sequence."},
        {utils::DecodeStatus::UnsupportedPixelFormat, "Unsupported pixel format."},
        {utils::DecodeStatus::PixelFormatMismatch, "Pixel format mismatch."},
        {utils::DecodeStatus::UnexpectedImageDimensions, "Unexpected image dimensions."},
    };
  });
  return sRegistry;
}

namespace utils {

using namespace std;

DecoderI::~DecoderI() = default;

DecoderFactory& DecoderFactory::get() {
  static DecoderFactory sInstance;
  return sInstance;
}

void DecoderFactory::registerDecoderMaker(DecoderMaker decoderMaker) {
  decoderMakers_.emplace_back(decoderMaker);
}

unique_ptr<DecoderI> DecoderFactory::makeDecoder(const string& codecName) {
  for (const DecoderMaker& decoderMaker : decoderMakers_) {
    std::unique_ptr<DecoderI> decoder = decoderMaker(codecName);
    if (decoder) {
      return decoder;
    }
  }
  XR_LOGW("Could not create a decoder for '{}'!", codecName);
  return nullptr;
}

} // namespace utils
} // namespace vrs
