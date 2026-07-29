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

struct DecoderOptions {
  bool useHwCodecs = true;
};

class DecoderI {
 public:
  DecoderI() = default;
  virtual ~DecoderI();
  DecoderI(const DecoderI&) = delete;
  DecoderI& operator=(const DecoderI&) = delete;
  DecoderI(DecoderI&&) = delete;
  DecoderI& operator=(DecoderI&&) = delete;
  /// Decode compressed image to a frame. outDecodedFrame is sized to
  /// outputImageSpec.getRawImageSize(); never write past it. Returns 0 on success, else a
  /// domainError(DecodeStatus::X) so errorCodeToMessage() can render it.
  virtual int decode(
      const vector<uint8_t>& encodedFrame,
      void* outDecodedFrame,
      const ImageContentBlockSpec& outputImageSpec) = 0;
  /// Flush the decoder's internal state (e.g., decoded picture buffer).
  /// Call this when seeking backward to avoid duplicate POC errors.
  /// Default implementation does nothing: custom image codecs are stateless and never need it,
  /// unlike video decoders that carry inter-frame state across seeks.
  virtual void flush() {}
};

/// Selects a DecoderI for a stream; it must only select, never decode, else the frame decodes
/// twice.
using DecoderMaker = std::function<std::unique_ptr<DecoderI>(
    const vector<uint8_t>& encodedFrame,
    void* outDecodedFrame,
    const ImageContentBlockSpec& outputImageSpec,
    const DecoderOptions& options)>;

class DecoderFactory {
 public:
  static DecoderFactory& get();

  /// Not thread-safe: register all makers at startup before any file is read. makeDecoder()
  /// iterates the maker list without a lock.
  void registerDecoderMaker(const DecoderMaker& decoderMaker);

  void registerDecoderMaker(
      std::unique_ptr<DecoderI> (
          *noOptionsDecoderMaker)(const vector<uint8_t>&, void*, const ImageContentBlockSpec&)) {
    registerDecoderMaker(
        DecoderMaker{[noOptionsDecoderMaker](
                         const vector<uint8_t>& encodedFrame,
                         void* outDecodedFrame,
                         const ImageContentBlockSpec& outputImageSpec,
                         const DecoderOptions& /*options*/) {
          return noOptionsDecoderMaker(encodedFrame, outDecodedFrame, outputImageSpec);
        }});
  }

  std::unique_ptr<DecoderI> makeDecoder(
      const vector<uint8_t>& encodedFrame,
      void* outDecodedFrame,
      const ImageContentBlockSpec& outputImageSpec);

  std::unique_ptr<DecoderI> makeDecoder(
      const vector<uint8_t>& encodedFrame,
      void* outDecodedFrame,
      const ImageContentBlockSpec& outputImageSpec,
      const DecoderOptions& options);

 protected:
  DecoderFactory() = default;

  std::vector<DecoderMaker> decoderMakers_;
};

} // namespace vrs::utils
