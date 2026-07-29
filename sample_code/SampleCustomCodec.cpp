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

#include <sample_code/SampleCustomCodec.h>

#include <algorithm>
#include <cstdint>

#include <vrs/ErrorCode.h>
#include <vrs/RecordFormat.h>
#include <vrs/utils/DecoderFactory.h>

using namespace vrs;
using namespace vrs::utils;

// Sample: make ImageFormat::CUSTOM_CODEC streams decodable by VRS tools. VRS bundles no custom
// codec; you register a DecoderI with DecoderFactory keyed on your codec name, and PixelFrame
// dispatches to it. This example "codec" is an identity codec whose blob is the raw pixels, to keep
// the focus on the wiring. Call registerSampleCustomCodecDecoder() once at startup.

namespace vrs_sample_code {

namespace {

constexpr const char* kSampleCodecName = "sample_identity_codec";

class SampleCustomCodecDecoder : public DecoderI {
 public:
  int decode(
      const vector<uint8_t>& encodedFrame,
      void* outDecodedFrame,
      const ImageContentBlockSpec& outputImageSpec) override {
    const size_t rawSize = ImageContentBlockSpec(
                               outputImageSpec.getPixelFormat(),
                               outputImageSpec.getWidth(),
                               outputImageSpec.getHeight())
                               .getRawImageSize();
    if (rawSize == ContentBlock::kSizeUnknown || rawSize == 0) {
      return domainError(DecodeStatus::UnexpectedImageDimensions);
    }
    // Identity codec: input is exactly the raw pixels. A real codec decompresses into
    // outDecodedFrame here; the invariant that carries over is to write at most rawSize bytes.
    if (encodedFrame.size() != rawSize) {
      return domainError(DecodeStatus::DecoderError);
    }
    std::copy_n(encodedFrame.data(), rawSize, static_cast<uint8_t*>(outDecodedFrame));
    return 0;
  }
};

} // namespace

void registerSampleCustomCodecDecoder() {
  DecoderFactory::get().registerDecoderMaker(
      [](const vector<uint8_t>& /*encodedFrame*/,
         void* /*outDecodedFrame*/,
         const ImageContentBlockSpec& outputImageSpec,
         const DecoderOptions& /*options*/) -> std::unique_ptr<DecoderI> {
        if (outputImageSpec.getCodecName() == kSampleCodecName) {
          return std::make_unique<SampleCustomCodecDecoder>();
        }
        return nullptr;
      });
}

} // namespace vrs_sample_code
