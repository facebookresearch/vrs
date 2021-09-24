// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <functional>
#include <memory>
#include <string>
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
  DecoderI() {}
  virtual ~DecoderI();
  /// Decode compressed image to a frame
  virtual int decode(
      RecordReader* reader,
      const uint32_t sizeBytes,
      void* outBuffer,
      const ImageContentBlockSpec& inputImageSpec) = 0;
  /// Decode compressed image, to update internal buffers
  virtual int decode(RecordReader* reader, const uint32_t sizeBytes) = 0;
};

using DecoderMaker = std::function<std::unique_ptr<DecoderI>(const std::string& codecName)>;

class DecoderFactory {
 public:
  static DecoderFactory& get();

  void registerDecoderMaker(DecoderMaker decoderMaker);

  std::unique_ptr<DecoderI> makeDecoder(const std::string& codecName);

 protected:
  DecoderFactory() = default;

  std::vector<DecoderMaker> decoderMakers_;
};

} // namespace vrs::utils
