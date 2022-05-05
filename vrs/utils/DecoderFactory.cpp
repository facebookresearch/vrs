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
const map<utils::DecodeStatus, const char*>& getErrorCodeRegistry<utils::DecodeStatus>() {
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
    unique_ptr<DecoderI> decoder = decoderMaker(codecName);
    if (decoder) {
      return decoder;
    }
  }
  XR_LOGW("Could not create a decoder for '{}'!", codecName);
  return nullptr;
}

} // namespace utils
} // namespace vrs
