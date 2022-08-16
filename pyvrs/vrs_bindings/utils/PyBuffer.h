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
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <pybind11/pybind11.h>

#include <vrs/RecordFormat.h>

namespace pyvrs {
namespace py = pybind11;
using namespace vrs;

/// \brief Image spec class.
///
/// This class is vrs::ImageContentBlockSpec for Python bindings.
/// The fields are exposed as a read only property but the methods required for dict interface is
/// supported so that user can treat PyImageContentBlockSpec object as Python Dictionary to iterate
/// on.
class PyImageContentBlockSpec {
 public:
  PyImageContentBlockSpec() = default;
  explicit PyImageContentBlockSpec(const ImageContentBlockSpec& spec) : spec_{spec} {}

  uint32_t getWidth() const {
    return spec_.getWidth();
  }

  uint32_t getHeight() const {
    return spec_.getHeight();
  }

  uint32_t getStride() const {
    if (spec_.getImageFormat() == ImageFormat::RAW) {
      return spec_.getStride();
    }
    return 0;
  }

  string getPixelFormatAsString() const {
    return spec_.getPixelFormatAsString();
  }

  string getImageFormatAsString() const {
    return spec_.getImageFormatAsString();
  }

  uint32_t getBytesPerPixel() const {
    if (spec_.getImageFormat() == ImageFormat::RAW) {
      return static_cast<uint32_t>(spec_.getBytesPerPixel());
    }
    return 0;
  }

  uint32_t getBlockSize() const {
    return static_cast<uint32_t>(spec_.getBlockSize());
  }

  size_t getRawImageSize() const {
    return spec_.getRawImageSize();
  }

  string getCodecName() const {
    return spec_.getCodecName();
  }

  uint8_t getCodecQuality() const {
    return spec_.getCodecQuality();
  }

  uint32_t getKeyFrameTimestamp() const {
    return spec_.getKeyFrameTimestamp();
  }

  uint32_t getKeyFrameIndex() const {
    return spec_.getKeyFrameIndex();
  }

  uint8_t getChannelCountPerPixel() const {
    if (spec_.getImageFormat() == ImageFormat::RAW) {
      return spec_.getChannelCountPerPixel();
    }
    return 0;
  }

  ImageFormat getImageFormat() const {
    return spec_.getImageFormat();
  }

  PixelFormat getPixelFormat() const {
    return spec_.getPixelFormat();
  }

  string asString() const {
    return spec_.asString();
  }

  ImageContentBlockSpec& getImageContentBlockSpec() {
    return spec_;
  }

  void initMap();
  map<string, py::object> map;

 private:
  ImageContentBlockSpec spec_;
};

/// \brief Audio spec class.
///
/// This class is vrs::AudioContentBlockSpec for Python bindings.
/// The fields are exposed as a read only property but the methods required for dict interface is
/// supported so that user can treat PyAudioContentBlockSpec object as Python Dictionary to iterate
/// on.
class PyAudioContentBlockSpec {
 public:
  explicit PyAudioContentBlockSpec(const AudioContentBlockSpec& spec) : spec_{spec} {}

  uint32_t getSampleCount() const {
    return spec_.getSampleCount();
  }

  string getSampleFormatAsString() const {
    return spec_.getSampleFormatAsString();
  }

  uint8_t getSampleBlockStride() const {
    return spec_.getSampleBlockStride();
  }

  uint8_t getChannelCount() const {
    return spec_.getChannelCount();
  }

  uint32_t getSampleRate() const {
    return spec_.getSampleRate();
  }

  uint32_t getBlockSize() const {
    return static_cast<uint32_t>(spec_.getBlockSize());
  }

  AudioSampleFormat getSampleFormat() const {
    return spec_.getSampleFormat();
  }

  void initMap();
  map<string, py::object> map;

 private:
  AudioContentBlockSpec spec_;
};

/// \brief Class for content block in VRS Record.
///
/// This class is vrs::ContentBlock for Python bindings.
/// The fields are exposed as a read only property but the methods required for dict interface is
/// supported so that user can treat ContentBlock object as Python Dictionary to iterate
/// on.
class PyContentBlock {
 public:
  explicit PyContentBlock(const ContentBlock& block) : block_{block} {}

  int getBufferSize() const {
    size_t blockSize = block_.getBlockSize();
    return blockSize == ContentBlock::kSizeUnknown ? -1 : static_cast<int>(blockSize);
  }

  void initMap();
  map<string, py::object> map;

 private:
  ContentBlock block_;
};

/// \brief Helper class to hold a content block binary data.
/// Exposed to Python using protocol_buffer, optimized using the content spec.
struct ContentBlockBuffer {
  explicit ContentBlockBuffer(const ContentBlock& block)
      : spec{block}, structuredArray{false}, bytesAdjusted{false} {}
  ContentBlock spec;
  vector<uint8_t> bytes;
  bool structuredArray; // should the buffer be returned as a 1 dimension uint8_t array?
  bool bytesAdjusted; // was the buffer endian swapped and/or realigned?
};

/// \brief Helper struct to hold an image data.
/// Exposed to Python using protocol_buffer, optimized using the content spec.
/// This class auto converts buffer to follow the pixel format on read.
struct ImageBuffer {
  ImageBuffer() = default;
  ImageBuffer(const PyImageContentBlockSpec& imageSpec, const vector<uint8_t>& imageBytes)
      : spec{imageSpec}, bytes{imageBytes} {}
  ImageBuffer(const PyImageContentBlockSpec& imageSpec, vector<uint8_t>&& bytes)
      : spec{imageSpec}, bytes{move(bytes)} {}
  ImageBuffer(const PyImageContentBlockSpec& imageSpec, const py::buffer& b) : spec{imageSpec} {
    initBytesFromPyBuffer(b);
  }
  ImageBuffer(const PyImageContentBlockSpec& imageSpec, int64_t recordIndex, const py::buffer& b)
      : spec{imageSpec}, recordIndex{recordIndex} {
    initBytesFromPyBuffer(b);
  }
  ImageBuffer(const ImageContentBlockSpec& imageSpec, const vector<uint8_t>& imageBytes)
      : spec{imageSpec}, bytes{imageBytes} {}
  ImageBuffer(const ImageContentBlockSpec& imageSpec, vector<uint8_t>&& bytes)
      : spec{imageSpec}, bytes{move(bytes)} {}
  ImageBuffer(const ImageContentBlockSpec& imageSpec, const py::buffer& b) : spec{imageSpec} {
    initBytesFromPyBuffer(b);
  }
  ImageBuffer(const ImageContentBlockSpec& imageSpec, int64_t recordIndex, const py::buffer& b)
      : spec{imageSpec}, recordIndex{recordIndex} {
    initBytesFromPyBuffer(b);
  }
  void initBytesFromPyBuffer(const py::buffer& b);

  PyImageContentBlockSpec spec;
  vector<uint8_t> bytes;
  int64_t recordIndex{-1}; // only used by AsyncImageFilter
};

/// \brief Helper struct to hold a binary data.
/// Exposed to Python using protocol_buffer, optimized using the content spec.
/// This class reads data as is without converting buffer.
struct BinaryBuffer {
  explicit BinaryBuffer(
      uint8_t* data_,
      size_t size_,
      size_t itemsize_,
      const string& format_,
      vector<size_t> shape_ = {})
      : data{data_}, size{size_}, itemsize{itemsize_}, format{format_}, shape{shape_} {
    // If shape is not specified, we treat this buffer as 1d array.
    if (shape.empty()) {
      shape.push_back(size);
    }
  }
  uint8_t* data;
  size_t size;
  size_t itemsize;
  string format;
  vector<size_t> shape;
};

/// Convert buffer to py::buffer_info following the spec.
py::buffer_info convertContentBlockBuffer(ContentBlockBuffer& block);
py::buffer_info convertImageBlockBuffer(ImageBuffer& block);

/// Binds methods and classes for PyBuffer.
void pybind_buffer(py::module& m);
} // namespace pyvrs
