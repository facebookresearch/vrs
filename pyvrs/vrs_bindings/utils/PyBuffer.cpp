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

#include "PyBuffer.h"
#include "vrs/RecordFormat.h"
#include "vrs/utils/PixelFrame.h"

#include <pybind11/detail/common.h>
#include <functional> // multiplies
#include <memory>
#include <numeric> // accumulate
#include <stdexcept>
#include <string>
#include <vector>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <vrs/helpers/Endian.h>
#include <vrs/os/Platform.h>

#define DEFAULT_LOG_CHANNEL "PyBuffer"
#include <logging/Log.h>
#include <logging/Verify.h>

#include "PyUtils.h"

namespace py = pybind11;

using namespace std;
using namespace vrs;
namespace {
const constexpr char* kImageSpecWidthKey = "width";
const constexpr char* kImageSpecHeightKey = "height";
const constexpr char* kImageSpecStrideKey = "stride";
const constexpr char* kImageSpecPixelFormatKey = "pixel_format";
const constexpr char* kImageSpecImageFormatKey = "image_format";
const constexpr char* kImageSpecBytesPerPixelKey = "bytes_per_pixel";
const constexpr char* kImageSpecBufferSizeKey = "buffer_size";
const constexpr char* kImageSpecCodecNameKey = "codec_name";
const constexpr char* kImageSpecCodecQualityKey = "codec_quality";
const constexpr char* kImageSpecKeyFrameTimestampKey = "key_frame_timestamp";
const constexpr char* kImageSpecKeyFrameIndexKey = "key_frame_index";

const constexpr char* kAudioSpecSampleCountKey = "sample_count";
const constexpr char* kAudioSpecSampleFormatKey = "sample_format";
const constexpr char* kAudioSpecSampleBlockStrideKey = "sample_block_stride";
const constexpr char* kAudioSpecChannelCountKey = "channel_count";
const constexpr char* kAudioSpecSampleRateKey = "sample_rate";
const constexpr char* kAudioSpecBufferSizeKey = "buffer_size";

const constexpr char* kContentBlockBufferSizeKey = "buffer_size";
} // namespace

namespace pyvrs {

ImageBuffer jxlCompress(
    const ImageContentBlockSpec& spec,
    const vector<uint8_t>& bytes,
    float quality,
    bool percentNotDistance = true) {
  if (!XR_VERIFY(spec.getImageFormat() == ImageFormat::RAW)) {
    // jxlCompression is only supported for ImageFormat::RAW.
    throw py::value_error(
        "Image format not supported for JXL compression: " + spec.getImageFormatAsString());
  }
  vector<uint8_t> outBuffer;
  if (!utils::PixelFrame::jxlCompress(spec, bytes, outBuffer, quality, percentNotDistance)) {
    // jxlCompression was unsuccessful. Throw an error.
    throw std::runtime_error("JXL compression unsuccessful.");
  }
  // Compression was successful. Contruct a new ImageBuffer with JXL image format and return
  return ImageBuffer(
      PyImageContentBlockSpec(ImageContentBlockSpec(
          ImageFormat::JXL, spec.getPixelFormat(), spec.getWidth(), spec.getHeight())),
      move(outBuffer));
}

ImageBuffer
jpgCompress(const ImageContentBlockSpec& spec, const vector<uint8_t>& bytes, uint32_t quality) {
  if (!XR_VERIFY(spec.getImageFormat() == ImageFormat::RAW)) {
    // jpgCompression is only supported for ImageFormat::RAW.
    throw py::value_error(
        "Image format not supported for JPG compression: " + spec.getImageFormatAsString());
  }
  vector<uint8_t> outBuffer;
  if (!utils::PixelFrame::jpgCompress(spec, bytes, outBuffer, quality)) {
    // jpgCompression was unsuccessful. Throw an error.
    throw std::runtime_error("JPG compression unsuccessful.");
  }
  // Compression was successful. Contruct a new ImageBuffer with JPG image format and return
  return ImageBuffer(
      PyImageContentBlockSpec(ImageContentBlockSpec(
          ImageFormat::JPG, spec.getPixelFormat(), spec.getWidth(), spec.getHeight())),
      move(outBuffer));
}

ImageBuffer decompress(const ImageContentBlockSpec& spec, const vector<uint8_t>& bytes) {
  utils::PixelFrame frame;
  if (!frame.readCompressedFrame(bytes, spec.getImageFormat())) {
    // Reading compressed format failed. Throw an error
    throw std::runtime_error("Reading compressed buffer failed.");
  }
  // Reading compressed frame was successful.
  return ImageBuffer(PyImageContentBlockSpec(frame.getSpec()), move(frame.getBuffer()));
}

void PyImageContentBlockSpec::initAttributesMap() {
  if (attributesMap.empty()) {
    attributesMap[kImageSpecWidthKey] = PYWRAP(getWidth());
    attributesMap[kImageSpecHeightKey] = PYWRAP(getHeight());
    attributesMap[kImageSpecStrideKey] = PYWRAP(getStride());
    attributesMap[kImageSpecPixelFormatKey] = PYWRAP(getPixelFormatAsString());
    attributesMap[kImageSpecImageFormatKey] = PYWRAP(getImageFormatAsString());
    attributesMap[kImageSpecBytesPerPixelKey] = PYWRAP(getBytesPerPixel());
    attributesMap[kImageSpecBufferSizeKey] = PYWRAP(getBlockSize());
    attributesMap[kImageSpecCodecNameKey] = PYWRAP(getCodecName());
    attributesMap[kImageSpecCodecQualityKey] = PYWRAP(getCodecQuality());
    attributesMap[kImageSpecKeyFrameTimestampKey] = PYWRAP(getKeyFrameTimestamp());
    attributesMap[kImageSpecKeyFrameIndexKey] = PYWRAP(getKeyFrameIndex());
  }
}

void PyAudioContentBlockSpec::initAttributesMap() {
  if (attributesMap.empty()) {
    attributesMap[kAudioSpecSampleCountKey] = PYWRAP(getSampleCount());
    attributesMap[kAudioSpecSampleFormatKey] = PYWRAP(getSampleFormatAsString());
    attributesMap[kAudioSpecSampleBlockStrideKey] = PYWRAP(getSampleBlockStride());
    attributesMap[kAudioSpecChannelCountKey] = PYWRAP(getChannelCount());
    attributesMap[kAudioSpecSampleRateKey] = PYWRAP(getSampleRate());
    attributesMap[kAudioSpecBufferSizeKey] = PYWRAP(getBlockSize());
  }
}

void PyContentBlock::initAttributesMap() {
  if (attributesMap.empty()) {
    attributesMap[kContentBlockBufferSizeKey] = PYWRAP(static_cast<uint32_t>(getBufferSize()));
  }
}

void ImageBuffer::initBytesFromPyBuffer(const py::buffer& b) {
  ImageFormat imageFormat = spec.getImageFormat();
  /* Request a buffer descriptor from Python */
  py::buffer_info info = b.request();
  size_t size = info.itemsize;
  for (py::ssize_t i = 0; i < info.ndim; i++) {
    size *= info.shape[i];
  }

  if (imageFormat == ImageFormat::RAW && size != spec.getRawImageSize()) {
    XR_LOGE(
        "Buffer size {} doesn't match the expected image size {}", size, spec.getRawImageSize());
    bytes.clear();
    return;
  }
  if (imageFormat == ImageFormat::UNDEFINED || imageFormat == ImageFormat::COUNT) {
    XR_LOGW("Invalid image format: {}", toString(imageFormat));
    bytes.clear();
    return;
  }

  const uint8_t* begin = reinterpret_cast<const uint8_t*>(info.ptr);

  bytes.assign(begin, begin + size);
}

py::buffer_info bufferInfoFromVector(vector<uint8_t>& vec) {
  return py::buffer_info(
      vec.data(), // Pointer to buffer
      static_cast<ssize_t>(sizeof(uint8_t)), // Size of one scalar
      py::format_descriptor<uint8_t>::format(), // Python struct-style format descriptor
      1, // Number of dimensions
      {static_cast<ssize_t>(vec.size())}, // Buffer dimensions
      {static_cast<ssize_t>(sizeof(uint8_t))}); // Strides (in bytes) for each index
}

ImageBuffer ImageBuffer::jxlCompress(float quality, bool percentNotDistance) const {
  return pyvrs::jxlCompress(spec.getImageContentBlockSpec(), bytes, quality, percentNotDistance);
}

ImageBuffer ImageBuffer::jpgCompress(uint32_t quality) const {
  return pyvrs::jpgCompress(spec.getImageContentBlockSpec(), bytes, quality);
}

ImageBuffer ImageBuffer::decompress() const {
  return pyvrs::decompress(spec.getImageContentBlockSpec(), bytes);
}

py::buffer_info convertContentBlockBuffer(ContentBlockBuffer& block) {
  if (block.structuredArray && block.spec.getContentType() == vrs::ContentType::IMAGE &&
      XR_VERIFY(block.bytes.size() == block.spec.image().getRawImageSize())) {
    const vrs::ImageContentBlockSpec& imageSpec = block.spec.image();
    uint32_t bytesPerPixel = static_cast<uint32_t>(imageSpec.getBytesPerPixel());
    uint32_t stride = imageSpec.getStride();
    uint32_t channelCount = imageSpec.getChannelCountPerPixel();
    uint32_t width = imageSpec.getWidth();

    // Get the python struct style format string. Do not implement a default to be explicit.
    std::string pixelFormat;
    switch (imageSpec.getPixelFormat()) {
      case vrs::PixelFormat::DEPTH32F:
      case vrs::PixelFormat::RGB32F:
      case vrs::PixelFormat::RGBA32F:
        pixelFormat = py::format_descriptor<float>::format();
        break;
      case vrs::PixelFormat::SCALAR64F:
        pixelFormat = py::format_descriptor<double>::format();
        break;
      case vrs::PixelFormat::GREY10:
      case vrs::PixelFormat::GREY12:
      case vrs::PixelFormat::GREY16:
      case vrs::PixelFormat::RGB10:
      case vrs::PixelFormat::RGB12:
        pixelFormat = py::format_descriptor<uint16_t>::format();
        break;
      case vrs::PixelFormat::YUV_I420_SPLIT:
        // We only pass the first plane for legacy reasons
        bytesPerPixel = 1;
        channelCount = 1;
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case vrs::PixelFormat::YUY2:
        // present the buffer in compact form
        bytesPerPixel = 2;
        channelCount = 2;
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case vrs::PixelFormat::RAW10:
      case vrs::PixelFormat::RAW10_BAYER_RGGB:
      case vrs::PixelFormat::RAW10_BAYER_BGGR:
        // expose raw buffer of bytes and let the user decode the RAW10 format.
        bytesPerPixel = 1;
        channelCount = 1;
        width = stride;
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case vrs::PixelFormat::GREY8:
      case vrs::PixelFormat::BGR8:
      case vrs::PixelFormat::RGB8:
      case vrs::PixelFormat::RGBA8:
      case vrs::PixelFormat::BAYER8_RGGB:
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case vrs::PixelFormat::UNDEFINED:
      case vrs::PixelFormat::COUNT:
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case vrs::PixelFormat::RGB_IR_RAW_4X4:
        // RGB_IR_RAW_4X4 packs 3 channels into one pixel. We present the buffer compressed, as if
        // there was only 1 channel.
        channelCount = 1;
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
    } // do not add a default, to force implementation when a new pixel format is added

    // Prepare a vector for the buffer dimensions and strides. For single channel images these
    // vectors have two items (rows & cols). For >1 channel images, three entries are
    // required.
    std::vector<uint32_t> shapes = {imageSpec.getHeight(), width};
    std::vector<uint32_t> stridesInBytes = {stride, bytesPerPixel};
    if (channelCount > 1) {
      shapes.push_back(channelCount);
      stridesInBytes.push_back(bytesPerPixel / channelCount);
    }

    return py::buffer_info(
        block.bytes.data(), // Pointer to buffer
        static_cast<ssize_t>(bytesPerPixel / channelCount), // Size of one scalar
        pixelFormat, // Python struct-style format descriptor
        static_cast<ssize_t>(shapes.size()), // Number of dimensions
        shapes, // Buffer dimensions
        stridesInBytes); // Strides (in bytes)
  }
  if (block.structuredArray && block.spec.getContentType() == vrs::ContentType::AUDIO) {
    const vrs::AudioContentBlockSpec& audioSpec = block.spec.audio();
    std::string sampleFormat;
    uint32_t audioSampleStride = audioSpec.getSampleBlockStride();
    uint32_t audioSampleCount = audioSpec.getSampleCount();
    uint32_t channelCount = audioSpec.getChannelCount();
    uint32_t audioSampleByteSize = audioSpec.getBytesPerSample();
    switch (audioSpec.getSampleFormat()) {
      case vrs::AudioSampleFormat::S8:
        sampleFormat = py::format_descriptor<int8_t>::format();
        break;
      case vrs::AudioSampleFormat::U8:
      case vrs::AudioSampleFormat::A_LAW:
      case vrs::AudioSampleFormat::MU_LAW:
        sampleFormat = py::format_descriptor<uint8_t>::format();
        break;
      case vrs::AudioSampleFormat::S16_LE:
      case vrs::AudioSampleFormat::S16_BE:
        sampleFormat = py::format_descriptor<int16_t>::format();
        break;
      case vrs::AudioSampleFormat::U16_LE:
      case vrs::AudioSampleFormat::U16_BE:
        sampleFormat = py::format_descriptor<uint16_t>::format();
        break;
      case vrs::AudioSampleFormat::S24_LE:
      case vrs::AudioSampleFormat::S24_BE:
      case vrs::AudioSampleFormat::U24_LE:
      case vrs::AudioSampleFormat::U24_BE: {
        // we create a new buffer, with 32 bit values, with 4-bytes alignment, little endian
        if (!block.bytesAdjusted) {
          bool needsEndianSwap = !audioSpec.isLittleEndian();
          std::vector<uint8_t> newBuffer;
          newBuffer.resize(4 * audioSpec.getSampleCount() * audioSpec.getChannelCount());
          const uint8_t* src = block.bytes.data();
          int32_t* dst = reinterpret_cast<int32_t*>(newBuffer.data());
          // Sample reconstruction buffer
          union {
            uint32_t uint32;
            int32_t int32;
            uint8_t bytes[4];
          } sample{0}; // top byte is cleared heare, and we never touch it
          for (uint32_t sampleIndex = 0; sampleIndex < audioSampleCount; sampleIndex++) {
            for (uint8_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
              if (needsEndianSwap) {
                sample.bytes[0] = src[channelIndex * 3 + 2];
                sample.bytes[1] = src[channelIndex * 3 + 1];
                sample.bytes[2] = src[channelIndex * 3 + 0];
              } else {
                sample.bytes[0] = src[channelIndex * 3 + 0];
                sample.bytes[1] = src[channelIndex * 3 + 1];
                sample.bytes[2] = src[channelIndex * 3 + 2];
              }
              dst[channelIndex] = sample.int32;
            }
            src += audioSampleStride;
            dst += channelCount;
          }
          block.bytes.swap(newBuffer);
          block.bytesAdjusted = true;
        }
        audioSampleByteSize = 4;
        audioSampleStride = audioSampleByteSize * channelCount;
        if (audioSpec.getSampleFormat() == vrs::AudioSampleFormat::S24_LE ||
            audioSpec.getSampleFormat() == vrs::AudioSampleFormat::S24_BE) {
          sampleFormat = py::format_descriptor<int32_t>::format();
        } else {
          sampleFormat = py::format_descriptor<uint32_t>::format();
        }
      } break;
      case vrs::AudioSampleFormat::S32_LE:
      case vrs::AudioSampleFormat::S32_BE:
        sampleFormat = py::format_descriptor<int32_t>::format();
        break;
      case vrs::AudioSampleFormat::U32_LE:
      case vrs::AudioSampleFormat::U32_BE:
        sampleFormat = py::format_descriptor<uint32_t>::format();
        break;
      case vrs::AudioSampleFormat::F32_LE:
      case vrs::AudioSampleFormat::F32_BE:
        sampleFormat = py::format_descriptor<float>::format();
        break;
      case vrs::AudioSampleFormat::F64_LE:
      case vrs::AudioSampleFormat::F64_BE:
        sampleFormat = py::format_descriptor<double>::format();
        break;
      case vrs::AudioSampleFormat::UNDEFINED:
      case vrs::AudioSampleFormat::COUNT:
        throw py::type_error("Unsupported audio sample format");
    }
    // check if we need to fix the audio buffer's endianness
    if (audioSampleStride > 1 && !block.bytesAdjusted && !audioSpec.isLittleEndian()) {
      uint8_t* bytes = block.bytes.data();
      if (audioSampleStride == 2) {
        for (uint32_t sampleIndex = 0; sampleIndex < audioSampleCount; sampleIndex++) {
          uint16_t* data = reinterpret_cast<uint16_t*>(bytes);
          for (uint8_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
            data[channelIndex] = be16toh(data[channelIndex]);
            data++;
          }
          bytes += audioSampleStride;
        }
      } else if (audioSampleStride == 4) {
        for (uint32_t sampleIndex = 0; sampleIndex < audioSampleCount; sampleIndex++) {
          uint32_t* data = reinterpret_cast<uint32_t*>(bytes);
          for (uint8_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
            data[channelIndex] = be32toh(data[channelIndex]);
            data++;
          }
          bytes += audioSampleStride;
        }
      } else if (audioSampleStride == 8) {
        for (uint32_t sampleIndex = 0; sampleIndex < audioSampleCount; sampleIndex++) {
          uint64_t* data = reinterpret_cast<uint64_t*>(bytes);
          for (uint8_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
            data[channelIndex] = be64toh(data[channelIndex]);
            data++;
          }
          bytes += audioSampleStride;
        }
      } else {
        // Should never happen, except for bugs in this code
        py::pybind11_fail("Unsupported sample stride during buffer endian swap");
      }
      block.bytesAdjusted = true;
    }
    return py::buffer_info(
        block.bytes.data(), // Pointer to buffer
        static_cast<ssize_t>(audioSampleByteSize), // Size of one scalar
        sampleFormat, // Python struct-style format descriptor
        2, // Number of dimensions
        {static_cast<ssize_t>(audioSampleCount),
         static_cast<ssize_t>(channelCount)}, // Buffer dimensions
        {static_cast<ssize_t>(audioSampleStride),
         static_cast<ssize_t>(audioSampleByteSize)}); // Strides (in bytes)
  }
  // unstructured array: just return the buffer as a simple one dimension byte array
  return bufferInfoFromVector(block.bytes);
}

ImageBuffer ContentBlockBuffer::jxlCompress(float quality, bool percentNotDistance) const {
  return pyvrs::jxlCompress(spec.image(), bytes, quality, percentNotDistance);
}

ImageBuffer ContentBlockBuffer::jpgCompress(uint32_t quality) const {
  return pyvrs::jpgCompress(spec.image(), bytes, quality);
}

ImageBuffer ContentBlockBuffer::decompress() const {
  return pyvrs::decompress(spec.image(), bytes);
}

py::buffer_info convertImageBlockBuffer(ImageBuffer& block) {
  const PyImageContentBlockSpec& imageSpec = block.spec;
  const ImageFormat imageFormat = imageSpec.getImageFormat();

  /// For jpg, png, video encoded images, we return buffer as 1d array.
  if (imageFormat == ImageFormat::JPG || imageFormat == ImageFormat::PNG ||
      imageFormat == ImageFormat::VIDEO) {
    return bufferInfoFromVector(block.bytes);
  }
  if (imageSpec.getImageFormat() == vrs::ImageFormat::RAW &&
      XR_VERIFY(block.bytes.size() == block.spec.getRawImageSize())) {
    uint32_t width = imageSpec.getWidth();
    uint32_t stride = imageSpec.getStride();
    size_t bytesPerPixel = imageSpec.getBytesPerPixel();
    uint32_t channelCount = imageSpec.getChannelCountPerPixel();

    // Get the python struct style format string. By default this is uint8_t and all
    // exceptions are explicitly listed in the switch statement for clarity.
    string pixelFormat;
    switch (imageSpec.getPixelFormat()) {
      case PixelFormat::DEPTH32F:
      case PixelFormat::RGB32F:
      case PixelFormat::RGBA32F:
        pixelFormat = py::format_descriptor<float>::format();
        break;
      case PixelFormat::SCALAR64F:
        pixelFormat = py::format_descriptor<double>::format();
        break;
      case PixelFormat::GREY10:
      case PixelFormat::GREY12:
      case PixelFormat::GREY16:
      case PixelFormat::RGB10:
      case PixelFormat::RGB12:
        pixelFormat = py::format_descriptor<uint16_t>::format();
        break;
      case PixelFormat::YUV_I420_SPLIT:
        // return first plane only
        bytesPerPixel = 1;
        channelCount = 1;
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case PixelFormat::YUY2:
        // present the buffer in compact form
        bytesPerPixel = 2;
        channelCount = 2;
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case PixelFormat::RAW10:
      case PixelFormat::RAW10_BAYER_RGGB:
      case PixelFormat::RAW10_BAYER_BGGR:
        // expose raw buffer of bytes and let the user decode the RAW10 format.
        bytesPerPixel = 1;
        channelCount = 1;
        width = stride;
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case PixelFormat::GREY8:
      case PixelFormat::BGR8:
      case PixelFormat::RGB8:
      case PixelFormat::RGBA8:
      case PixelFormat::RGB_IR_RAW_4X4:
      case PixelFormat::BAYER8_RGGB:
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
      case PixelFormat::UNDEFINED:
      case PixelFormat::COUNT:
        pixelFormat = py::format_descriptor<uint8_t>::format();
        break;
    }

    if (bytesPerPixel == ContentBlock::kSizeUnknown) {
      XR_LOGE("bytesPerPixel for this pixel format is undefined");
    }

    // Prepare a vector for the buffer dimensions and strides. For single
    // channel images these vectors have two items (rows & cols).
    // For >1 channel images, three entries are required.
    vector<uint32_t> shapes = {imageSpec.getHeight(), width};
    vector<uint32_t> stridesInBytes = {stride, static_cast<uint32_t>(bytesPerPixel)};
    if (channelCount > 1) {
      shapes.push_back(channelCount);
      stridesInBytes.push_back(bytesPerPixel / channelCount);
    }

    return py::buffer_info(
        block.bytes.data(), // Pointer to buffer
        static_cast<ssize_t>(bytesPerPixel / channelCount), // Size of one scalar
        pixelFormat, // Python struct-style format descriptor
        static_cast<ssize_t>(shapes.size()), // Number of dimensions
        shapes, // Buffer dimensions
        stridesInBytes); // Strides (in bytes)
  } else {
    XR_LOGW("Invalid image format: {}", toString(imageFormat));
    return bufferInfoFromVector(block.bytes);
  }
}

#if IS_VRS_OSS_CODE()
void pybind_buffer(py::module& m) {
  auto imageSpec =
      py::class_<PyImageContentBlockSpec>(m, "ImageSpec")
          .def_property_readonly(kImageSpecWidthKey, &PyImageContentBlockSpec::getWidth)
          .def_property_readonly(kImageSpecHeightKey, &PyImageContentBlockSpec::getHeight)
          .def_property_readonly(kImageSpecStrideKey, &PyImageContentBlockSpec::getStride)
          .def_property_readonly(
              kImageSpecPixelFormatKey,
              [](const PyImageContentBlockSpec& spec) { return spec.getPixelFormatAsString(); })
          .def_property_readonly(
              kImageSpecImageFormatKey,
              [](const PyImageContentBlockSpec& spec) { return spec.getImageFormatAsString(); })
          .def_property_readonly(
              kImageSpecBytesPerPixelKey,
              [](const PyImageContentBlockSpec& spec) {
                return static_cast<uint32_t>(spec.getBytesPerPixel());
              })
          .def_property_readonly(kImageSpecBufferSizeKey, &PyImageContentBlockSpec::getBlockSize)
          .def_property_readonly(kImageSpecCodecNameKey, &PyImageContentBlockSpec::getCodecName)
          .def_property_readonly(
              kImageSpecCodecQualityKey, &PyImageContentBlockSpec::getCodecQuality)
          .def_property_readonly(
              kImageSpecKeyFrameTimestampKey, &PyImageContentBlockSpec::getKeyFrameTimestamp)
          .def_property_readonly(
              kImageSpecKeyFrameIndexKey, &PyImageContentBlockSpec::getKeyFrameIndex)
          .def("get_pixel_format", &PyImageContentBlockSpec::getPixelFormat)
          .def("get_image_format", &PyImageContentBlockSpec::getImageFormat)
          .def("get_width", &PyImageContentBlockSpec::getWidth)
          .def("get_height", &PyImageContentBlockSpec::getHeight)
          .def("get_stride", &PyImageContentBlockSpec::getStride);
  DEF_DICT_FUNC(imageSpec, PyImageContentBlockSpec)

  auto audioSpec =
      py::class_<PyAudioContentBlockSpec>(m, "AudioSpec")
          .def_property_readonly(kAudioSpecSampleCountKey, &PyAudioContentBlockSpec::getSampleCount)
          .def_property_readonly(
              kAudioSpecSampleFormatKey, &PyAudioContentBlockSpec::getSampleFormat)
          .def_property_readonly(
              kAudioSpecSampleBlockStrideKey, &PyAudioContentBlockSpec::getSampleBlockStride)
          .def_property_readonly(
              kAudioSpecChannelCountKey, &PyAudioContentBlockSpec::getChannelCount)
          .def_property_readonly(kAudioSpecSampleRateKey, &PyAudioContentBlockSpec::getSampleRate)
          .def_property_readonly(kAudioSpecBufferSizeKey, &PyAudioContentBlockSpec::getBlockSize);
  DEF_DICT_FUNC(audioSpec, PyAudioContentBlockSpec)

  auto contentBlock = py::class_<PyContentBlock>(m, "ContentBlock")
                          .def_property_readonly("buffer_size", &PyContentBlock::getBufferSize);
  DEF_DICT_FUNC(contentBlock, PyContentBlock)

  py::class_<pyvrs::ContentBlockBuffer>(m, "Buffer", py::buffer_protocol())
      .def("jxl_compress", &pyvrs::ContentBlockBuffer::jxlCompress)
      .def("jpg_compress", &pyvrs::ContentBlockBuffer::jpgCompress)
      .def("decompress", &pyvrs::ContentBlockBuffer::decompress)
      .def_buffer([](pyvrs::ContentBlockBuffer& block) -> py::buffer_info {
        return pyvrs::convertContentBlockBuffer(block);
      });

  py::class_<pyvrs::ImageBuffer>(m, "ImageBuffer", py::buffer_protocol())
      .def(py::init<const PyImageContentBlockSpec&, const py::buffer&>())
      .def(py::init<const PyImageContentBlockSpec&, int64_t, const py::buffer&>())
      .def("jxl_compress", &pyvrs::ImageBuffer::jxlCompress)
      .def("jpg_compress", &pyvrs::ImageBuffer::jpgCompress)
      .def("decompress", &pyvrs::ImageBuffer::decompress)
      .def_readonly("spec", &pyvrs::ImageBuffer::spec)
      .def_readonly("bytes", &pyvrs::ImageBuffer::bytes)
      .def_readwrite("record_index", &pyvrs::ImageBuffer::recordIndex)
      // only exposes block.bytes
      .def_buffer([](pyvrs::ImageBuffer& block) -> py::buffer_info {
        return pyvrs::convertImageBlockBuffer(block);
      });

  py::class_<pyvrs::BinaryBuffer>(m, "BinaryBuffer", py::buffer_protocol())
      .def_buffer([](pyvrs::BinaryBuffer& b) -> py::buffer_info {
        size_t size = b.shape.size();
        vector<ssize_t> shape, strides;
        shape.resize(size);
        strides.resize(size);
        shape[size - 1] = b.shape.back();
        strides[size - 1] = static_cast<ssize_t>(b.itemsize);
        for (int i = size - 2; i >= 0; i--) {
          shape[i] = b.shape[i];
          strides[i] = strides[i + 1] * b.shape[i + 1];
        }
        return py::buffer_info(
            b.data, static_cast<ssize_t>(b.itemsize), b.format, size, shape, strides);
      });
}
#endif
} // namespace pyvrs
