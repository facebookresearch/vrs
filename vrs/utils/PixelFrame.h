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

#include <cstdint>

#include <vector>

// Make this class usable both when Qt is available, and when not
#ifdef QOBJECT_H
#include <QtCore/QSize>
#endif

#include <vrs/RecordFormat.h>
#include <vrs/RecordReaders.h>
#include <vrs/utils/DecoderFactory.h>
#include <vrs/utils/VideoFrameHandler.h>

namespace vrs::utils {

/// Helper class to read & convert images read using RecordFormat into simpler, but maybe degraded,
/// pixel buffer, that can easily be displayed, or saved to disk as jpg or png.
///
/// Here are the "conversions" performed overall:
/// - GREY10, GREY12 and GREY16 to GREY8, by pixel depth reduction.
/// - RGB10 and RGB12 to RGB8, by pixel depth reduction.
/// - YUV_I420_SPLIT and YUY2 to RGB8, by conversion.
/// - DEPTH32F and SCALAR64F to GREY8, by normalization.
///
/// RGB32F is not currently not supported.
class PixelFrame {
 public:
  PixelFrame() = default;
  PixelFrame(const ImageContentBlockSpec& spec);
  PixelFrame(PixelFormat pf, uint32_t w, uint32_t h, uint32_t stride = 0)
      : PixelFrame(ImageContentBlockSpec(pf, w, h, stride)) {}

  void init(const ImageContentBlockSpec& spec);
  inline void init(PixelFormat pf, uint32_t w, uint32_t h, uint32_t stride = 0) {
    init(ImageContentBlockSpec(pf, w, h, stride));
  }

  static void init(std::shared_ptr<PixelFrame>& inOutFrame, const ImageContentBlockSpec& spec);
  static inline void init(
      std::shared_ptr<PixelFrame>& inOutFrame,
      PixelFormat pf,
      uint32_t w,
      uint32_t h,
      uint32_t stride = 0) {
    init(inOutFrame, {pf, w, h, stride});
  }

#ifdef QOBJECT_H
  QSize qsize() const {
    return QSize(static_cast<int>(getWidth()), static_cast<int>(getHeight()));
  }
#endif

  void swap(PixelFrame& other);

  const ImageContentBlockSpec& getSpec() const {
    return imageSpec_;
  }
  PixelFormat getPixelFormat() const {
    return imageSpec_.getPixelFormat();
  }
  uint32_t getWidth() const {
    return imageSpec_.getWidth();
  }
  uint32_t getHeight() const {
    return imageSpec_.getHeight();
  }
  size_t getStride() const {
    return imageSpec_.getStride();
  }

  std::vector<uint8_t>& getBuffer() {
    return frameBytes_;
  }
  const uint8_t* rdata() const {
    return frameBytes_.data();
  }
  uint8_t* wdata() {
    return frameBytes_.data();
  }
  size_t size() const {
    return frameBytes_.size();
  }
  uint8_t* getLine(uint32_t line) {
    return frameBytes_.data() + imageSpec_.getStride() * line;
  }
  bool hasSamePixels(const ImageContentBlockSpec& spec) const;

  /// Clear the pixel buffer
  void blankFrame();

  /// Read a RAW, PNG, or JPEG encoded frame into the internal buffer.
  /// @return True if the frame type is supported & the frame was read.
  static bool
  readFrame(std::shared_ptr<PixelFrame>& frame, RecordReader* reader, const ContentBlock& cb);

  /// Read a RAW frame into the internal buffer.
  /// @return True if the frame type is supported & the frame was read.
  bool readRawFrame(RecordReader* reader, const ImageContentBlockSpec& inputImageSpec);

  static bool readRawFrame(
      std::shared_ptr<PixelFrame>& frame,
      RecordReader* reader,
      const ImageContentBlockSpec& inputImageSpec);

  /// Read a JPEG encoded frame into the internal buffer.
  /// @return True if the frame type is supported & the frame was read.
  bool readJpegFrame(RecordReader* reader, const uint32_t sizeBytes);
  /// Decode a JPEG encoded frame into the internal buffer.
  /// @param buffer: jpg data, possibly read from a valid jpg file (the whole file).
  /// @param decodePixels: if true, decode the image in the buffer, otherwise, only read the format.
  /// @return True if the frame type is supported & the frame was read.
  bool readJpegFrame(const std::vector<uint8_t>& buffer, bool decodePixels = true);

  static bool
  readJpegFrame(std::shared_ptr<PixelFrame>& frame, RecordReader* reader, const uint32_t sizeBytes);

  /// Read a PNG encoded frame into the internal buffer.
  /// @param reader: The record reader to read data from.
  /// @param sizeBytes: Number of bytes to read from the reader.
  /// @return True if the frame type is supported & the frame was read.
  bool readPngFrame(RecordReader* reader, const uint32_t sizeBytes);
  /// Decode a png buffer data into the image.
  /// @param pngBuffer: png data, possibly read from a valid png file (the whole file).
  /// @param decodePixels: if true, decode the image in the buffer, otherwise, only read the format.
  /// @return True if the frame type is supported & the frame was read.
  bool readPngFrame(const std::vector<uint8_t>& pngBuffer, bool decodePixels = true);

  /// Save image as PNG
  /// @param path: path of the file to write
  /// @return A status code, 0 meaning success.
  int writeAsPng(const string& path);

  static bool
  readPngFrame(std::shared_ptr<PixelFrame>& frame, RecordReader* reader, const uint32_t sizeBytes);

  /// Normalize an input frame if possible and as necessary,
  /// which means it has one of the following pixel formats:
  /// - PixelFormat::RGB8 (if necessary)
  /// - PixelFormat::GREY8
  /// - PixelFormat::GREY16 (if allowed and useful)
  /// These pixel formats are useful because they are supported for display (except GREY16) and PNG
  /// for image extraction.
  /// @param sourceFrame: the frame to normalize. Won't be modifed.
  /// @param outFrame: the normalized frame, or the input frame.
  /// @param grey16supported: true to allow PixelFormat::GREY16, otherwise, GREY8 will be used.
  /// @return Nothing, but outFrame is set to:
  /// - sourceFrame if no conversion was necessary
  /// - sourceFrame if no conversion was possible
  /// - some new frame, if the frame was converted into a normalized format.
  static void normalizeFrame(
      const std::shared_ptr<PixelFrame>& sourceFrame,
      std::shared_ptr<PixelFrame>& outFrame,
      bool grey16supported);

  /// Convert the internal frame to a simpler format, if necessary.
  /// Returns false if the frame could not be converted, or the format doesn't need conversion.
  bool normalizeFrame(std::shared_ptr<PixelFrame>& normalizedFrame, bool grey16supported) const;

  static PixelFormat getNormalizedPixelFormat(PixelFormat sourcePixelFormat, bool grey16supported);

 private:
  /// Conversion from an external buffer
  /// @param convertedFrame: frame to convert to. May not be allocated yet.
  /// @param targetPixelFormat: pixel format to target. Expect GREY8, GREY16 and RGB8 to work.
  /// @return True if the conversion happened, otherwise, the buffer is left in an undefined state.
  /// Note that the actual conversion format is set in imageSpec_, and it could be different...
  bool normalizeToPixelFormat(std::shared_ptr<PixelFrame>& outFrame, PixelFormat targetPixelFormat)
      const;

 private:
  ImageContentBlockSpec imageSpec_;
  std::vector<uint8_t> frameBytes_;
};

} // namespace vrs::utils
