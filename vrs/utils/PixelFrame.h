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

#include <vector>

// Make this class usable both when Qt is available, and when not
#ifdef QOBJECT_H
#include <QtCore/QSize>
#endif

#include <vrs/ForwardDefinitions.h>
#include <vrs/RecordFormat.h>
#include <vrs/RecordReaders.h>
#include <vrs/utils/DecoderFactory.h>
#include <vrs/utils/PixelFrameOptions.h>
#include <vrs/utils/VideoFrameHandler.h>

namespace vrs::utils {

using std::shared_ptr;
using std::vector;

/// Helper class to read & convert images read using RecordFormat into simpler, but maybe degraded,
/// pixel buffer, that can easily be displayed, or saved to disk as jpg or png.
///
/// Here are some of the "normalizations" performed:
/// - GREY10, GREY12 and GREY16 to GREY8, by pixel depth reduction.
/// - RGB10 and RGB12 to RGB8, by pixel depth reduction.
/// - YUV_I420_SPLIT and YUY2 to RGB8, by conversion.
/// - DEPTH32F and SCALAR64F to GREY8, by normalization.
///
class PixelFrame {
 public:
  PixelFrame() = default;
  explicit PixelFrame(const ImageContentBlockSpec& spec);
  PixelFrame(ImageContentBlockSpec spec, vector<uint8_t>&& frameBytes);
  PixelFrame(PixelFormat pf, uint32_t w, uint32_t h, uint32_t stride = 0);

  void init(const ImageContentBlockSpec& spec);
  void init(const ImageContentBlockSpec& spec, vector<uint8_t>&& frameBytes);
  void init(PixelFormat pf, uint32_t w, uint32_t h, uint32_t stride = 0, uint32_t stride2 = 0);

  // helpers to make sure the unique_ptr or shared_ptr is not nullptr
  // and makes sure the frame is initialized with the given spec.
  template <class T>
  static inline PixelFrame& init(T& inOutFramePtr, const ImageContentBlockSpec& spec) {
    if (!inOutFramePtr) {
      inOutFramePtr.reset(new PixelFrame(spec));
    } else {
      inOutFramePtr->init(spec);
    }
    return *inOutFramePtr;
  }
  template <class T>
  static inline PixelFrame&
  init(T& inOutFramePtr, PixelFormat pf, uint32_t w, uint32_t h, uint32_t stride = 0) {
    if (!inOutFramePtr) {
      inOutFramePtr.reset(new PixelFrame(pf, w, h, stride));
    } else {
      inOutFramePtr->init(pf, w, h, stride);
    }
    return *inOutFramePtr;
  }

#ifdef QOBJECT_H
  QSize qsize() const {
    return QSize(static_cast<int>(getWidth()), static_cast<int>(getHeight()));
  }
#endif

  void swap(PixelFrame& other) noexcept;

  const ImageContentBlockSpec& getSpec() const {
    return imageSpec_;
  }
  ImageFormat getImageFormat() const {
    return imageSpec_.getImageFormat();
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
  uint32_t getStride() const {
    return imageSpec_.getStride();
  }
  uint32_t getDefaultStride() const {
    return imageSpec_.getDefaultStride();
  }
  uint32_t getPlaneStride(uint32_t planeIndex) const {
    return imageSpec_.getPlaneStride(planeIndex);
  }
  uint32_t getPlaneHeight(uint32_t planeIndex) const {
    return imageSpec_.getPlaneHeight(planeIndex);
  }
  uint8_t getChannelCountPerPixel() const {
    return imageSpec_.getChannelCountPerPixel();
  }
  size_t getBytesPerPixel() const {
    return imageSpec_.getBytesPerPixel();
  }

  vector<uint8_t>& getBuffer() {
    return frameBytes_;
  }
  const uint8_t* rdata() const {
    return frameBytes_.data();
  }
  uint8_t* wdata() {
    return frameBytes_.data();
  }
  template <class T>
  inline const T* data(size_t byte_offset = 0) const {
    return reinterpret_cast<const T*>(frameBytes_.data() + byte_offset);
  }
  template <class T>
  inline T* data(size_t byte_offset = 0) {
    return reinterpret_cast<T*>(frameBytes_.data() + byte_offset);
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
  template <class T>
  static inline bool readFrame(T& framePtr, RecordReader* reader, const ContentBlock& cb) {
    return init(framePtr, cb.image()).readFrame(reader, cb);
  }
  bool readFrame(RecordReader* reader, const ContentBlock& cb);

  /// Read a record's image data, merely reading the disk data without any decompression.
  /// The resulting PixelFrame will have an unmodified ImageFormat (raw, jpg, png, jxl, video).
  template <class T>
  static inline bool readDiskImageData(T& framePtr, RecordReader* reader, const ContentBlock& cb) {
    return init(framePtr, cb.image()).readDiskImageData(reader, cb);
  }
  bool readDiskImageData(RecordReader* reader, const ContentBlock& cb);

  /// From any ImageFormat, decompress the image to ImageFormat::RAW if necessary.
  /// To decompress ImageFormat::VIDEO data, you must provide a valid VideoFrameHandler object, the
  /// same one for all the frames of a particular stream.
  bool decompressImage(VideoFrameHandler* videoFrameHandler = nullptr);

  /// Read a RAW frame into the internal buffer.
  /// @return True if the frame type is supported & the frame was read.
  template <class T>
  static inline bool
  readRawFrame(T& framePtr, RecordReader* reader, const ImageContentBlockSpec& inputImageSpec) {
    return init(framePtr, inputImageSpec).readRawFrame(reader, inputImageSpec);
  }
  bool readRawFrame(RecordReader* reader, const ImageContentBlockSpec& inputImageSpec);

  /// Decode compressed image data, except for video codec compression.
  bool readCompressedFrame(const vector<uint8_t>& pixels, ImageFormat imageFormat);

  /// Read a JPEG encoded frame into the internal buffer.
  /// @return True if the frame type is supported & the frame was read.
  bool readJpegFrame(RecordReader* reader, uint32_t sizeBytes);
  /// Decode a JPEG encoded frame into the internal buffer.
  /// @param buffer: jpg data, possibly read from a valid jpg file (the whole file).
  /// @param decodePixels: if true, decode the image in the buffer, otherwise, only read the format.
  /// @return True if the frame type is supported & the frame was read.
  bool readJpegFrame(const vector<uint8_t>& buffer, bool decodePixels = true);
  /// Decode a JPEG encoded frame into the internal buffer.
  /// @param path: path to jpg file
  /// @param decodePixels: if true, decode the image in the buffer, otherwise, only read the format.
  /// @return True if the frame type is supported & the frame was read.
  bool readJpegFrameFromFile(const string& path, bool decodePixels = true);

  template <class T>
  static inline bool readJpegFrame(T& framePtr, RecordReader* reader, uint32_t sizeBytes) {
    return make(framePtr).readJpegFrame(reader, sizeBytes);
  }

  /// Compress pixel frame to jpg. Supports ImageFormat::RAW and PixelFormat::RGB8 or GREY8 only.
  /// @param outBuffer: on exit, the jpg payload which can be saved as a .jpg file
  /// @param quality: jpg quality setting, from 1 to 100
  /// @return True if the image and pixel formats are supported, the compression succeeded, and
  /// outBuffer was set. If returning False, do not use outBuffer.
  bool jpgCompress(vector<uint8_t>& outBuffer, uint32_t quality);

  /// Compress pixel frame to jpg. See other variant of jpgCompress for specs.
  /// @param pixelSpec: specs of the pixel buffer.
  /// @param pixels: the raw pixel buffer.
  /// @param outBuffer: on exit, the jpg payload which can be saved as a .jpg file.
  /// outBuffer may be the same as pixels.
  /// @param quality: jpg quality setting, from 1 to 100
  /// @return True if the image and pixel formats are supported, the compression succeeded, and
  /// outBuffer was set. If returning False, do not use outBuffer.
  static bool jpgCompress(
      const ImageContentBlockSpec& pixelSpec,
      const vector<uint8_t>& pixels,
      vector<uint8_t>& outBuffer,
      uint32_t quality);

  /// Same as previous version, with an external pixel buffer.
  static bool jpgCompress(
      const ImageContentBlockSpec& pixelSpec,
      const uint8_t* pixels,
      vector<uint8_t>& outBuffer,
      uint32_t quality);

  /// Read a JPEG-XL encoded frame into the internal buffer.
  /// @return True if the frame type is supported & the frame was read.
  /// Returns false, if no decoder was installed, or the data couldn't be decoded correctly.
  bool readJxlFrame(RecordReader* reader, uint32_t sizeBytes);
  /// Decode a JPEG-XL encoded frame into the internal buffer.
  /// @param buffer: jpeg-xl data, possibly read from a valid jxl file (the whole file).
  /// @param decodePixels: if true, decode the image in the buffer, otherwise, only read the format.
  /// @return True if the frame type is supported & the frame was read.
  /// Returns false, if no decoder was installed, or the data couldn't be decoded correctly.
  bool readJxlFrame(const vector<uint8_t>& buffer, bool decodePixels = true);

  /// Compress pixel frame to jxl. Supports ImageFormat::RAW and PixelFormat::RGB8 or GREY8 only.
  /// @param outBuffer: on exit, the jxl payload which can be saved as a .jxl file
  /// @param quality: jxl quality setting, from 20 to 100 for percentage, or 0 to 15 for distance.
  /// @param options: compression options. See CompressionOptions for details.
  /// @return True if the image and pixel formats are supported, the compression succeeded, and
  /// outBuffer was set. If returning False, do not use outBuffer.
  bool
  jxlCompress(vector<uint8_t>& outBuffer, float quality, const CompressionOptions& options = {});

  /// Compress pixel frame to jxl. Supports ImageFormat::RAW and PixelFormat::RGB8 or GREY8 only.
  /// @param pixelSpec: specs of the pixel buffer.
  /// @param pixels: the raw pixel buffer.
  /// @param outBuffer: on exit, the jxl payload which can be saved as a .jxl file.
  /// outBuffer may be the same as pixels.
  /// @param quality: jxl quality setting, from 20 to 100 for percentage, or 0 to 15 for distance.
  /// @param options: compression options. See CompressionOptions for details.
  /// @return True if the image and pixel formats are supported, the compression succeeded, and
  /// outBuffer was set. If returning False, do not use outBuffer.
  static bool jxlCompress(
      const ImageContentBlockSpec& pixelSpec,
      const vector<uint8_t>& pixels,
      vector<uint8_t>& outBuffer,
      float quality,
      const CompressionOptions& options = {});

  /// Read a PNG encoded frame into the internal buffer.
  /// @param reader: The record reader to read data from.
  /// @param sizeBytes: Number of bytes to read from the reader.
  /// @return True if the frame type is supported & the frame was read.
  bool readPngFrame(RecordReader* reader, uint32_t sizeBytes);
  /// Decode a png buffer data into the image.
  /// @param pngBuffer: png data, possibly read from a valid png file (the whole file).
  /// @param decodePixels: if true, decode the image in the buffer, otherwise, only read the format.
  /// @return True if the frame type is supported & the frame was read.
  bool readPngFrame(const vector<uint8_t>& pngBuffer, bool decodePixels = true);

  template <class T>
  static inline bool readPngFrame(T& framePtr, RecordReader* reader, uint32_t sizeBytes) {
    return make(framePtr).readPngFrame(reader, sizeBytes);
  }

  /// Save image as PNG
  /// @param path: path of the file to write, if no outBuffer is provided
  /// @param outBuffer: if provided, a vector where to write the data, instead of writing to disk
  /// @return A status code, 0 meaning success.
  int writeAsPng(const string& path, vector<uint8_t>* outBuffer = nullptr) const;

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
      const shared_ptr<PixelFrame>& sourceFrame,
      shared_ptr<PixelFrame>& outFrame,
      bool grey16supported,
      const NormalizeOptions& options = {});

  /// Convert the internal frame to a simpler format, if necessary.
  /// Returns false if the frame could not be converted, or the format doesn't need conversion.
  bool normalizeFrame(
      shared_ptr<PixelFrame>& outNormalizedFrame,
      bool grey16supported,
      const NormalizeOptions& options = {}) const;

  /// Convert the internal frame to a simpler format, if necessary.
  /// Returns false if the frame could not be converted, or the format doesn't need conversion.
  /// @param outNormalizedFrame: on exit, the normalized frame. Input value is ignored.
  /// @param grey16supported: true to allow PixelFormat::GREY16.
  /// @param normalizedPixelFormat: if you computed it, the normalized pixel format.
  /// Only valid for what getNormalizedPixelFormat() returns! (to avoid double-calculations)
  bool normalizeFrame(
      PixelFrame& outNormalizedFrame,
      bool grey16supported,
      const NormalizeOptions& options = {},
      PixelFormat normalizedPixelFormat = PixelFormat::UNDEFINED) const;

  /// Get normalized pixel format for a given pixel format and options.
  static PixelFormat getNormalizedPixelFormat(
      PixelFormat sourcePixelFormat,
      bool grey16supported,
      const NormalizeOptions& options = {});

  /// Tell if an image format supports a specific pixel format.
  /// Only meaningful for png, jpg, jxl.
  /// Always true for ImageFormat::VIDEO (needs decoders to be sure).
  /// Always false for ImageFormat::CUSTOM_CODEC (never available).
  static bool areCompatible(ImageFormat imageFormat, PixelFormat pixelFormat);

  /// Conversion in place from RGBA to RGB (no memory allocation)
  /// @return True if the conversion was performed, false if source wasn't an RGBA frame.
  bool inplaceRgbaToRgb();

  /// Convert current RGBA frame to RGB, with allocation of a PixelFrame, if necessary.
  /// @param outRgbFrame: a shared pointer to a PixelFrame. Does not need to be allocated yet.
  /// @return True if the conversion was performed, false if source wasn't an RGBA frame.
  bool convertRgbaToRgb(shared_ptr<PixelFrame>& outRgbFrame) const;

  /// Compare this image with another image, and return a PSNR score.
  /// @param other: the other image to compare with.
  /// @param msssim: on exit, the PSNR.
  /// @return True if the images have identical dimensions and pixel formats, the pixel format is
  /// supported (grey8 or rgb8), and the comparison succeeded.
  bool psnrCompare(const PixelFrame& other, double& outPsnr);

  /// Compare this image with another image, and return an MS-SSIM score.
  /// @param other: the other image to compare with.
  /// @param msssim: on exit, the MS-SSIM score.
  /// @return True if the images have identical dimensions and pixel formats, the pixel format is
  /// supported (grey8 or rgb8), and the comparison succeeded.
  bool msssimCompare(const PixelFrame& other, double& msssim);

  static NormalizeOptions
  getStreamNormalizeOptions(RecordFileReader& reader, StreamId id, PixelFormat format);

  struct RGBColor {
    RGBColor() = default;
    RGBColor(uint8_t r, uint8_t g, uint8_t b) : r{r}, g{g}, b{b} {}
    uint8_t r{}, g{}, b{};
  };
  static const vector<RGBColor>& getGetObjectIdSegmentationColors();
  static const vector<RGBColor>& getGetObjectClassSegmentationColors();

  /// Get the name of a particular segmentation class from its index
  static const char* getSegmentationClassName(uint16_t classIndex);
  /// Print the segmentation colors used since the last reset, with color samples
  static void printSegmentationColors();
  /// Clear the segmentation colors seen so far (loading a new file?)
  static void clearSegmentationColors();

  // Helper to make sure a unique_ptr or shared_ptr is set to a valid PixelFrame,
  // but doesn't guarantee the frame is initialized to any particular state.
  template <class T>
  static inline PixelFrame& make(T& inOutFramePtr) {
    if (!inOutFramePtr) {
      inOutFramePtr.reset(new PixelFrame());
    }
    return *inOutFramePtr;
  }

 private:
  /// Conversion from an external buffer
  /// @param outNormalizedFrame: on exit, converted frame when returning true.
  /// @param targetPixelFormat: pixel format to target. Expect GREY8, GREY16 and RGB8 to work.
  /// @return True if the conversion happened, otherwise, the buffer is left in an undefined state.
  /// Note that the actual conversion format is set in imageSpec_, and it could be different...
  bool normalizeToPixelFormat(
      PixelFrame& outNormalizedFrame,
      PixelFormat targetPixelFormat,
      const NormalizeOptions& options) const;
  bool normalizeToPixelFormatWithOcean(
      PixelFrame& outFrame,
      PixelFormat targetPixelFormat,
      const NormalizeOptions& options) const;

 private:
  ImageContentBlockSpec imageSpec_;
  vector<uint8_t> frameBytes_;
};

} // namespace vrs::utils
