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

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <vrs/RecordFormat.h>
#include <vrs/RecordReaders.h>

namespace vrs::utils {

class PixelFrame;

enum class DecodeStatus {
  DecoderError = 1,
  CodecNotFound,
  FrameSequenceError,
  UnsupportedPixelFormat,
  PixelFormatMismatch,
  UnexpectedImageDimensions,
};

class DecoderI {
 public:
  DecoderI() = default;
  virtual ~DecoderI();
  /// Decode compressed image to a frame
  virtual int decode(
      const vector<uint8_t>& encodedFrame,
      void* outDecodedFrame,
      const ImageContentBlockSpec& outputImageSpec) = 0;
};

using DecoderMaker = std::function<std::unique_ptr<DecoderI>(
    const vector<uint8_t>& encodedFrame,
    void* outDecodedFrame,
    const ImageContentBlockSpec& outputImageSpec)>;

class DecoderFactory {
 public:
  static DecoderFactory& get();

  void registerDecoderMaker(const DecoderMaker& decoderMaker);

  std::unique_ptr<DecoderI> makeDecoder(
      const vector<uint8_t>& encodedFrame,
      void* outDecodedFrame,
      const ImageContentBlockSpec& outputImageSpec);

 protected:
  DecoderFactory() = default;

  std::vector<DecoderMaker> decoderMakers_;
};

} // namespace vrs::utils
