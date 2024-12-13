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

#include "RecordFormat.h"

#include <cassert>

#include <array>
#include <limits>
#include <sstream>
#include <tuple>
#include <utility>

#define DEFAULT_LOG_CHANNEL "RecordFormat"
#include <logging/Log.h>
#include <logging/Verify.h>

#include <vrs/DataLayout.h>
#include <vrs/helpers/EnumStringConverter.h>

using namespace std;

/// RecordFormat string parsing helper class. @internal
class vrs::ContentParser {
 public:
  ContentParser(const string& parseString, char delim) : is_(parseString), delim_{delim} {
    next();
  }

  string str; // The text piece being looked at

  string source() const {
    return is_.str(); // The whole original
  }

  bool next() {
    if (is_.eof()) {
      str.clear();
    } else {
      getline(is_, str, delim_);
    }
    return !str.empty();
  }

 private:
  istringstream is_;
  char delim_;
};

const size_t vrs::ContentBlock::kSizeUnknown = numeric_limits<size_t>::max();

namespace {

using namespace vrs;

// These text names may NEVER BE CHANGED, as they are used in data layout definitions!!
const char* sContentTypeNames[] = {"custom", "empty", "data_layout", "image", "audio"};
struct ContentTypeFormatConverter : public EnumStringConverter<
                                        ContentType,
                                        sContentTypeNames,
                                        COUNT_OF(sContentTypeNames),
                                        ContentType::CUSTOM,
                                        ContentType::CUSTOM> {
  static_assert(cNamesCount == enumCount<ContentType>(), "Missing ContentType name definitions");
};

// These text names may NEVER BE CHANGED, as they are used in data layout definitions!!
const char* sImageFormatNames[] = {"undefined", "raw", "jpg", "png", "video", "jxl"};
struct ImageFormatConverter : public EnumStringConverter<
                                  ImageFormat,
                                  sImageFormatNames,
                                  COUNT_OF(sImageFormatNames),
                                  ImageFormat::UNDEFINED,
                                  ImageFormat::UNDEFINED> {
  static_assert(cNamesCount == enumCount<ImageFormat>(), "Missing ImageFormat name definitions");
};

// Enum values may NEVER BE CHANGED, as they are used in data layout definitions!!
static_assert(static_cast<int>(ImageFormat::RAW) == 1, "ImageFormat enum values CHANGED!");
static_assert(static_cast<int>(ImageFormat::JPG) == 2, "ImageFormat enum values CHANGED!");
static_assert(static_cast<int>(ImageFormat::PNG) == 3, "ImageFormat enum values CHANGED!");
static_assert(static_cast<int>(ImageFormat::VIDEO) == 4, "ImageFormat enum values CHANGED!");
static_assert(static_cast<int>(ImageFormat::JXL) == 5, "ImageFormat enum values CHANGED!");

// These text names may NEVER BE CHANGED, as they are used in data layout definitions!!
const char* sPixelFormatNames[] = {
    "undefined",        "grey8",        "bgr8",        "depth32f",  "rgb8",
    "yuv_i420_split",   "rgba8",        "rgb10",       "rgb12",     "grey10",
    "grey12",           "grey16",       "rgb32F",      "scalar64F", "yuy2",
    "rgb_ir_4x4",       "rgba32F",      "bayer8_rggb", "raw10",     "raw10_bayer_rggb",
    "raw10_bayer_bggr", "yuv_420_nv21", "yuv_420_nv12"};

struct PixelFormatConverter : public EnumStringConverter<
                                  PixelFormat,
                                  sPixelFormatNames,
                                  COUNT_OF(sPixelFormatNames),
                                  PixelFormat::UNDEFINED,
                                  PixelFormat::UNDEFINED> {
  static_assert(cNamesCount == enumCount<PixelFormat>(), "Missing PixelFormat name definitions");
};

// Enum values may NEVER BE CHANGED, as they are used in data layout definitions!!
// We're testing some key values, but NONE of the declared values may be changed, ever!
static_assert(static_cast<int>(PixelFormat::GREY8) == 1, "PixelFormat enum values CHANGED!");
static_assert(
    static_cast<int>(PixelFormat::YUV_I420_SPLIT) == 5,
    "PixelFormat enum values CHANGED!");
static_assert(static_cast<int>(PixelFormat::RGBA8) == 6, "PixelFormat enum values CHANGED!");
static_assert(static_cast<int>(PixelFormat::GREY10) == 9, "PixelFormat enum values CHANGED!");
static_assert(static_cast<int>(PixelFormat::RGB32F) == 12, "PixelFormat enum values CHANGED!");
static_assert(static_cast<int>(PixelFormat::YUY2) == 14, "PixelFormat enum values CHANGED!");
static_assert(static_cast<int>(PixelFormat::RGBA32F) == 16, "PixelFormat enum values CHANGED!");
static_assert(static_cast<int>(PixelFormat::RAW10) == 18, "PixelFormat enum values CHANGED!");
static_assert(
    static_cast<int>(PixelFormat::RAW10_BAYER_RGGB) == 19,
    "PixelFormat enum values CHANGED!");
static_assert(
    static_cast<int>(PixelFormat::RAW10_BAYER_BGGR) == 20,
    "PixelFormat enum values CHANGED!");
static_assert(
    static_cast<int>(PixelFormat::YUV_420_NV21) == 21,
    "PixelFormat enum values CHANGED!");
static_assert(
    static_cast<int>(PixelFormat::YUV_420_NV12) == 22,
    "PixelFormat enum values CHANGED!");
const char* sAudioFormatNames[] = {"undefined", "pcm", "opus"};
struct AudioFormatConverter : public EnumStringConverter<
                                  AudioFormat,
                                  sAudioFormatNames,
                                  COUNT_OF(sAudioFormatNames),
                                  AudioFormat::UNDEFINED,
                                  AudioFormat::UNDEFINED> {
  static_assert(cNamesCount == enumCount<AudioFormat>(), "Missing AudioFormat name definitions");
};

// Enum values may NEVER BE CHANGED, as they are used in data layout definitions!!
static_assert(static_cast<int>(AudioFormat::PCM) == 1, "AudioFormat enum values CHANGED!");
static_assert(static_cast<int>(AudioFormat::OPUS) == 2, "AudioFormat enum values CHANGED!");

// These text names may NEVER BE CHANGED, as they are used in data layout definitions!!
const char* sAudioSampleFormatNames[] = {
    "undefined", "int8",     "uint8",    "uint8alaw", "uint8mulaw", "int16le",   "uint16le",
    "int16be",   "uint16be", "int24le",  "uint24le",  "int24be",    "uint24be",  "int32le",
    "uint32le",  "int32be",  "uint32be", "float32le", "float32be",  "float64le", "float64be"};
struct AudioSampleFormatConverter : public EnumStringConverter<
                                        AudioSampleFormat,
                                        sAudioSampleFormatNames,
                                        COUNT_OF(sAudioSampleFormatNames),
                                        AudioSampleFormat::UNDEFINED,
                                        AudioSampleFormat::UNDEFINED> {
  static_assert(
      cNamesCount == enumCount<AudioSampleFormat>(),
      "Missing AudioSampleFormat name definitions");
};

// Enum values may NEVER BE CHANGED, as they are used in data layout definitions!!
// We're testing some key values, but NONE of the declared values may be changed, ever!
static_assert(static_cast<int>(AudioSampleFormat::S8) == 1, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::U8) == 2, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::S16_LE) == 5, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::U16_LE) == 6, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::S24_LE) == 9, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::S32_LE) == 13, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::U32_LE) == 14, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::F32_LE) == 17, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::F64_LE) == 19, "Enum values CHANGED!");
static_assert(static_cast<int>(AudioSampleFormat::F64_BE) == 20, "Enum values CHANGED!");

#define CHAR_TO_INT(c) static_cast<int>(static_cast<uint8_t>(c))
char lowXdigit(int i) {
  char c = static_cast<char>(i & 0x0F);
  return c < 10 ? '0' + c : 'A' + c - 10;
}
#define ISXDIGIT(c) isxdigit(CHAR_TO_INT(c))
char xdigitToChar(char xdigit) {
  return (xdigit <= '9') ? xdigit - '0' : 10 + xdigit - ((xdigit <= 'Z') ? 'A' : 'a');
}

string escapeString(const string& str) {
  string s;
  s.reserve(str.size() + str.size() / 10 + 20);
  const char* kEscapeChars = "+/\\% \"'";
  for (char c : str) {
    if (c < 32 || c >= 127 || strchr(kEscapeChars, c) != nullptr) {
      s.push_back('%');
      s.push_back(lowXdigit(c >> 4));
      s.push_back(lowXdigit(c));
    } else {
      s.push_back(c);
    }
  }
  return s;
}

string unescapeString(const char* str, size_t length) {
  string s;
  s.reserve(length);
  for (size_t k = 0; k < length && str[k] != 0; ++k) {
    char c = str[k];
    if ((c == '%') && k + 2 < length && ISXDIGIT(str[k + 1]) && ISXDIGIT(str[k + 2])) {
      c = (xdigitToChar(str[k + 1]) << 4) | xdigitToChar(str[k + 2]);
      k += 2;
    }
    s.push_back(c);
  }
  return s;
}

} // namespace

namespace vrs {

template <>
ImageFormat toEnum<>(const string& name) {
  return ImageFormatConverter::toEnumNoCase(name.c_str());
}

template <>
PixelFormat toEnum<>(const string& name) {
  return PixelFormatConverter::toEnumNoCase(name.c_str());
}

template <>
AudioFormat toEnum<>(const string& name) {
  return AudioFormatConverter::toEnumNoCase(name.c_str());
}

template <>
AudioSampleFormat toEnum<>(const string& name) {
  return AudioSampleFormatConverter::toEnumNoCase(name.c_str());
}

ImageContentBlockSpec::ImageContentBlockSpec(
    const ImageContentBlockSpec& imageSpec,
    double keyFrameTimestamp,
    uint32_t keyFrameIndex)
    : ImageContentBlockSpec(imageSpec) {
  keyFrameTimestamp_ = keyFrameTimestamp;
  keyFrameIndex_ = keyFrameIndex;
}

ImageContentBlockSpec::ImageContentBlockSpec(
    ImageFormat imageFormat,
    uint32_t width,
    uint32_t height) {
  imageFormat_ = imageFormat;
  width_ = width;
  height_ = height;
}

ImageContentBlockSpec::ImageContentBlockSpec(
    PixelFormat pixelFormat,
    uint32_t width,
    uint32_t height,
    uint32_t stride,
    uint32_t stride2)
    : imageFormat_{ImageFormat::RAW},
      pixelFormat_{pixelFormat},
      width_{width},
      height_{height},
      stride_{stride},
      stride2_{stride2} {
  sanityCheckStrides();
}

ImageContentBlockSpec::ImageContentBlockSpec(
    ImageFormat imageFormat,
    PixelFormat pixelFormat,
    uint32_t width,
    uint32_t height,
    uint32_t stride,
    uint32_t stride2,
    string codecName,
    uint8_t codecQuality,
    double keyFrameTimestamp,
    uint32_t keyFrameIndex)
    : imageFormat_{imageFormat},
      pixelFormat_{pixelFormat},
      width_{width},
      height_{height},
      stride_{stride},
      stride2_{stride2},
      codecName_{std::move(codecName)},
      keyFrameTimestamp_{keyFrameTimestamp},
      keyFrameIndex_{keyFrameIndex},
      codecQuality_{codecQuality} {
  sanityCheckStrides();
}

ImageContentBlockSpec::ImageContentBlockSpec(
    string codecName,
    uint8_t codecQuality,
    PixelFormat pixelFormat,
    uint32_t width,
    uint32_t height,
    uint32_t stride,
    uint32_t stride2)
    : imageFormat_{ImageFormat::VIDEO},
      pixelFormat_{pixelFormat},
      width_{width},
      height_{height},
      stride_{stride},
      stride2_{stride2},
      codecName_{std::move(codecName)},
      codecQuality_{codecQuality} {
  sanityCheckStrides();
}

ImageContentBlockSpec::ImageContentBlockSpec(const string& formatStr) {
  ContentParser parser(formatStr, '/');
  set(parser);
  sanityCheckStrides();
}

void ImageContentBlockSpec::clear() {
  imageFormat_ = ImageFormat::UNDEFINED;
  pixelFormat_ = PixelFormat::UNDEFINED;
  width_ = 0;
  height_ = 0;
  stride_ = 0;
  stride2_ = 0;
  codecName_.clear();
  codecQuality_ = kQualityUndefined;
}

ImageContentBlockSpec ImageContentBlockSpec::core() const {
  return {
      imageFormat_,
      pixelFormat_,
      width_,
      height_,
      stride_ > 0 && stride_ != getDefaultStride() ? stride_ : 0,
      stride2_ > 0 && stride2_ != getDefaultStride2() ? stride2_ : 0,
      codecName_};
}

bool ImageContentBlockSpec::operator==(const ImageContentBlockSpec& rhs) const {
  auto tie = [](const ImageContentBlockSpec& v) {
    return std::tie(
        v.imageFormat_,
        v.pixelFormat_,
        v.width_,
        v.height_,
        v.stride_,
        v.stride2_,
        v.codecName_,
        v.codecQuality_,
        v.keyFrameTimestamp_,
        v.keyFrameIndex_);
  };
  return tie(*this) == tie(rhs);
}

bool ImageContentBlockSpec::operator!=(const ImageContentBlockSpec& rhs) const {
  return !operator==(rhs);
}

void ImageContentBlockSpec::set(ContentParser& parser) {
  clear();
  if (parser.str.empty()) {
    return;
  }
  imageFormat_ = ImageFormatConverter::toEnum(parser.str.c_str());
  if (imageFormat_ == ImageFormat::UNDEFINED) {
    XR_LOGE("Could not parse image format '{}' in '{}'", parser.str, parser.source());
  } else {
    uint32_t width{}, height{}, stride{}, quality{}, keyFrameIndex{};
    double keyFrameTimestamp = 0;
    array<char, 200> text = {};
    while (parser.next()) {
      const char* cstr = parser.str.c_str();
      int firstChar = static_cast<uint8_t>(*cstr); // could be 0, but that's ok
      if (isdigit(firstChar) != 0 && width_ == 0 && sscanf(cstr, "%ux%u", &width, &height) == 2) {
        width_ = width;
        height_ = height;
      } else if (
          firstChar == 'p' && pixelFormat_ == PixelFormat::UNDEFINED &&
          parser.str.size() < text.size() && sscanf(cstr, "pixel=%s", text.data()) == 1) {
        pixelFormat_ = PixelFormatConverter::toEnum(text.data());
      } else if (firstChar == 's' && stride_ == 0 && sscanf(cstr, "stride=%u", &stride) == 1) {
        stride_ = stride;
      } else if (firstChar == 's' && stride2_ == 0 && sscanf(cstr, "stride_2=%u", &stride) == 1) {
        stride2_ = stride;
      } else if (
          firstChar == 'c' && codecName_.empty() && parser.str.size() < text.size() &&
          sscanf(cstr, "codec=%s", text.data()) == 1) {
        codecName_ = unescapeString(text.data(), text.size());
      } else if (firstChar == 'c' && sscanf(cstr, "codec_quality=%u", &quality) == 1) {
        uint8_t q = static_cast<uint8_t>(quality);
        codecQuality_ = isQualityValid(q) ? q : kQualityUndefined;
      } else if (
          firstChar == 'k' && sscanf(cstr, "keyframe_timestamp=%lf", &keyFrameTimestamp) == 1) {
        keyFrameTimestamp_ = keyFrameTimestamp;
      } else if (firstChar == 'k' && sscanf(cstr, "keyframe_index=%u", &keyFrameIndex) == 1) {
        keyFrameIndex_ = keyFrameIndex;
      } else {
        XR_LOGE("Could not parse image spec '{}' in '{}'", parser.str, parser.source());
      }
    }
  }
}

string ImageContentBlockSpec::asString() const {
  if (imageFormat_ == ImageFormat::UNDEFINED) {
    return {};
  }
  string s;
  s.reserve(120);
  s.append(ImageFormatConverter::toString(imageFormat_));
  if (width_ > 0 && height_ > 0) {
    s.append("/").append(to_string(width_)).append("x").append(to_string(height_));
  }
  if (imageFormat_ == ImageFormat::RAW || imageFormat_ == ImageFormat::VIDEO) {
    if (pixelFormat_ != PixelFormat::UNDEFINED) {
      s.append("/pixel=").append(PixelFormatConverter::toString(pixelFormat_));
    }
    if (stride_ > 0) {
      s.append("/stride=").append(to_string(stride_));
    }
    if (stride2_ > 0) {
      s.append("/stride_2=").append(to_string(stride2_));
    }
    if (imageFormat_ == ImageFormat::VIDEO) {
      if (!codecName_.empty()) {
        s.append("/codec=").append(escapeString(codecName_));
      }
      if (isQualityValid(codecQuality_)) {
        s.append("/codec_quality=").append(to_string(codecQuality_));
      }
      if (keyFrameTimestamp_ != kInvalidTimestamp) {
        // These conversions will only be needed for debugging, so precision issues are ok.
        // Using 9 for up to nanosecond precision.
        s.append("/keyframe_timestamp=")
            .append(fmt::format("{:.9f}", keyFrameTimestamp_))
            .append("/keyframe_index=")
            .append(to_string(keyFrameIndex_));
      }
    }
  }
  return s;
}

uint8_t ImageContentBlockSpec::getChannelCountPerPixel(PixelFormat pixel) {
  switch (pixel) {
    case PixelFormat::GREY8:
    case PixelFormat::GREY10:
    case PixelFormat::GREY12:
    case PixelFormat::GREY16:
    case PixelFormat::DEPTH32F:
    case PixelFormat::SCALAR64F:
    case PixelFormat::BAYER8_RGGB:
    case PixelFormat::RAW10_BAYER_RGGB:
    case PixelFormat::RAW10_BAYER_BGGR:
    case PixelFormat::RAW10:
      return 1; // grayscale, or "depth", or any form of single numeric value per pixel
    case PixelFormat::BGR8:
    case PixelFormat::RGB8:
    case PixelFormat::RGB10:
    case PixelFormat::RGB12:
    case PixelFormat::RGB32F:
    case PixelFormat::RGB_IR_RAW_4X4:
    case PixelFormat::YUV_I420_SPLIT:
    case PixelFormat::YUY2:
    case PixelFormat::YUV_420_NV21:
    case PixelFormat::YUV_420_NV12:
      return 3; // Typically Red/Green/Blue, or YUV of some kind
    case PixelFormat::RGBA8:
    case PixelFormat::RGBA32F:
      return 4; // Typically 3 color channels plus transparency

    // Every actual pixel format should have a count of channels,
    // which is unrelated to the memory representation of the pixel data, packed or not.
    // Don't use default to force every enum value to be explicitly specified
    case PixelFormat::UNDEFINED:
    case PixelFormat::COUNT:
      return 0;
  }
  return 0; // required by some compilers
}

size_t ImageContentBlockSpec::getBytesPerPixel(PixelFormat pixel) {
  switch (pixel) {
    case PixelFormat::GREY8:
    case PixelFormat::RGB_IR_RAW_4X4:
    case PixelFormat::BAYER8_RGGB:
      return 1;
    case PixelFormat::GREY10:
    case PixelFormat::GREY12:
    case PixelFormat::GREY16:
      return 2;
    case PixelFormat::RGB8:
    case PixelFormat::BGR8:
      return 3;
    case PixelFormat::DEPTH32F:
    case PixelFormat::RGBA8:
      return 4;
    case PixelFormat::RGB10:
    case PixelFormat::RGB12:
      return 6;
    case PixelFormat::SCALAR64F:
      return 8;
    case PixelFormat::RGB32F:
      return 12;
    case PixelFormat::RGBA32F:
      return 16;

    // Not every pixel format stores data in successive bytes and fits the mold. Do not pretend.
    // Don't use default to force every enum value to be explicitly specified
    case PixelFormat::YUY2:
    case PixelFormat::YUV_I420_SPLIT:
    case PixelFormat::RAW10:
    case PixelFormat::RAW10_BAYER_RGGB:
    case PixelFormat::RAW10_BAYER_BGGR:
    case PixelFormat::YUV_420_NV21:
    case PixelFormat::YUV_420_NV12:
    case PixelFormat::UNDEFINED:
    case PixelFormat::COUNT:
      return ContentBlock::kSizeUnknown;
  }
  return ContentBlock::kSizeUnknown; // required by some compilers
}

uint32_t ImageContentBlockSpec::getPlaneCount(PixelFormat pixelFormat) {
  switch (pixelFormat) {
    case PixelFormat::YUV_I420_SPLIT:
      return 3;
    case PixelFormat::YUV_420_NV21:
    case PixelFormat::YUV_420_NV12:
      return 2;
    default:
      return 1;
  }
  return 1;
}

bool ImageContentBlockSpec::sanityCheckStrides() const {
  bool allGood = true;
  if (stride_ > 0 && stride_ < getDefaultStride()) {
    XR_LOGE("Invalid stride for {}. Minimum stride: {}.", asString(), getDefaultStride());
    allGood = false;
  }
  if (stride2_ > 0 && stride2_ < getDefaultStride2()) {
    XR_LOGE("Invalid stride2 for {}. Minimum stride2: {}.", asString(), getDefaultStride2());
    allGood = false;
  }
  return allGood;
}

uint32_t ImageContentBlockSpec::getPlaneStride(uint32_t planeIndex) const {
  if (planeIndex == 0) {
    return getStride();
  }
  if (planeIndex >= getPlaneCount(pixelFormat_)) {
    return 0;
  }
  return stride2_ > 0 ? stride2_ : getDefaultStride2();
}

uint32_t ImageContentBlockSpec::getDefaultStride2() const {
  switch (pixelFormat_) {
    case PixelFormat::YUV_I420_SPLIT:
      // second and third planes use one byte per 2-2 squares: half the width, half the height
      return (getWidth() + 1) / 2;
      break;
    case PixelFormat::YUV_420_NV21:
    case PixelFormat::YUV_420_NV12:
      // one pair U+V for each 2x2 block of pixels
      return getWidth() + (getWidth() % 2);
      break;
    default:
      return 0;
  }
  return 0;
}

uint32_t ImageContentBlockSpec::getPlaneHeight(uint32_t planeIndex) const {
  if (planeIndex == 0) {
    return getHeight();
  }
  if (planeIndex >= getPlaneCount()) {
    return 0;
  }
  switch (pixelFormat_) {
    case PixelFormat::YUV_I420_SPLIT:
      // second and third planes use one byte per 2-2 squares: half the width, half the height
      return (getHeight() + 1) / 2;
    case PixelFormat::YUV_420_NV21:
    case PixelFormat::YUV_420_NV12:
      return (getHeight() + 1) / 2;
    default:
      return 0;
  }
  return 0;
}

size_t ImageContentBlockSpec::getBlockSize() const {
  return (imageFormat_ == ImageFormat::RAW) ? getRawImageSize() : ContentBlock::kSizeUnknown;
}

size_t ImageContentBlockSpec::getRawImageSize() const {
  if (pixelFormat_ != PixelFormat::UNDEFINED && width_ > 0 && height_ > 0) {
    size_t size = 0;
    const uint32_t planeCount = getPlaneCount();
    for (uint32_t plane = 0; plane < planeCount; ++plane) {
      size += getPlaneStride(plane) * getPlaneHeight(plane);
    }
    if (size > 0) {
      return size;
    }
  }
  return ContentBlock::kSizeUnknown;
}

string ImageContentBlockSpec::getPixelFormatAsString() const {
  return PixelFormatConverter::toString(pixelFormat_);
}

string ImageContentBlockSpec::getImageFormatAsString() const {
  return ImageFormatConverter::toString(imageFormat_);
}

uint32_t ImageContentBlockSpec::getStride() const {
  // Use the explicitly set stride if available.
  return stride_ > 0 ? stride_ : getDefaultStride();
}

uint32_t ImageContentBlockSpec::getDefaultStride() const {
  // Try to compute the stride using the bytes per pixel.
  size_t bytesPerPixel = getBytesPerPixel();
  if (bytesPerPixel != ContentBlock::kSizeUnknown) {
    return getWidth() * static_cast<uint32_t>(bytesPerPixel);
  }
  switch (pixelFormat_) {
    case PixelFormat::YUV_I420_SPLIT:
    case PixelFormat::YUV_420_NV21:
    case PixelFormat::YUV_420_NV12:
      return getWidth();
    case PixelFormat::RAW10:
    case PixelFormat::RAW10_BAYER_RGGB:
    case PixelFormat::RAW10_BAYER_BGGR: {
      // groups of 4 pixels use 5 bytes, sharing the 5th for their last two bits
      uint32_t fourPixelsGroupCount = (getWidth() + 3) / 4;
      return fourPixelsGroupCount * 5;
    }
    case PixelFormat::YUY2: {
      // groups of 2 pixels store their data in 4 bytes
      uint32_t twoPixelsGroupCount = (getWidth() + 1) / 2;
      return twoPixelsGroupCount * 4;
    }
    case PixelFormat::UNDEFINED: {
      return 0;
    }
    default:;
  }
  // Every pixel format must compute a default stride when none is explicitly provided
  XR_LOGE("The pixel format {} isn't properly implemented.", toString(pixelFormat_));
  return 0;
}

AudioContentBlockSpec::AudioContentBlockSpec(AudioFormat audioFormat, uint8_t channelCount) {
  clear();
  audioFormat_ = audioFormat;
  channelCount_ = channelCount;
}

AudioContentBlockSpec::AudioContentBlockSpec(
    AudioFormat audioFormat,
    AudioSampleFormat sampleFormat,
    uint8_t channelCount,
    uint8_t sampleFrameStride,
    uint32_t sampleFrameRate,
    uint32_t sampleFrameCount,
    uint8_t stereoPairCount)
    : audioFormat_{audioFormat},
      sampleFormat_{sampleFormat},
      sampleFrameStride_{sampleFrameStride},
      channelCount_{channelCount},
      sampleFrameRate_{sampleFrameRate},
      sampleFrameCount_{sampleFrameCount},
      stereoPairCount_{stereoPairCount} {
  XR_VERIFY(audioFormat != AudioFormat::UNDEFINED);
  XR_VERIFY(sampleFrameStride_ == 0 || sampleFrameStride_ >= getBytesPerSample() * channelCount);
  XR_VERIFY(channelCount >= stereoPairCount * 2);
}

AudioContentBlockSpec::AudioContentBlockSpec(const string& formatStr) {
  ContentParser parser(formatStr, '/');
  set(parser);
}

void AudioContentBlockSpec::clear() {
  audioFormat_ = AudioFormat::UNDEFINED;
  sampleFormat_ = AudioSampleFormat::UNDEFINED;
  channelCount_ = 0;
  sampleFrameStride_ = 0;
  sampleFrameRate_ = 0;
  sampleFrameCount_ = 0;
}

void AudioContentBlockSpec::set(ContentParser& parser) {
  clear();
  if (parser.str.empty()) {
    return;
  }
  audioFormat_ = AudioFormatConverter::toEnum(parser.str.c_str());
  if (audioFormat_ == AudioFormat::UNDEFINED) {
    XR_LOGE("Could not parse audio format '{}' in '{}'", parser.str, parser.source());
  } else {
    unsigned int tmp = 0;
    while (parser.next()) {
      // peek at first character
      switch (parser.str[0]) {
        case 'i':
        case 'u':
        case 'f': // first letters of known sample formats
          if (sampleFormat_ == AudioSampleFormat::UNDEFINED) {
            sampleFormat_ = AudioSampleFormatConverter::toEnum(parser.str.c_str());
            if (sampleFormat_ != AudioSampleFormat::UNDEFINED) {
              continue;
            }
          }
          break;
        case 'c':
          if (channelCount_ == 0 && sscanf(parser.str.c_str(), "channels=%u", &tmp) == 1) {
            channelCount_ = static_cast<uint8_t>(tmp);
            continue;
          }
          break;
        case 'r':
          if (sampleFrameRate_ == 0 && sscanf(parser.str.c_str(), "rate=%u", &tmp) == 1) {
            sampleFrameRate_ = static_cast<uint32_t>(tmp);
            continue;
          }
          break;
        case 's':
          if (sampleFrameCount_ == 0 && sscanf(parser.str.c_str(), "samples=%u", &tmp) == 1) {
            sampleFrameCount_ = static_cast<uint32_t>(tmp);
            continue;
          } else if (
              sampleFrameStride_ == 0 && sscanf(parser.str.c_str(), "stride=%u", &tmp) == 1) {
            sampleFrameStride_ = static_cast<uint8_t>(tmp);
            continue;
          }
          break;
      }
      XR_LOGE("Could not parse audio spec '{}' in '{}'", parser.str, parser.source());
    }
  }
}

string AudioContentBlockSpec::asString() const {
  if (audioFormat_ == AudioFormat::UNDEFINED) {
    return {};
  }
  string s;
  s.reserve(120);
  s.append(AudioFormatConverter::toString(audioFormat_));
  if (sampleFormat_ != AudioSampleFormat::UNDEFINED) {
    s.append("/").append(AudioSampleFormatConverter::toString(sampleFormat_));
  }
  if (channelCount_ != 0) {
    s.append("/channels=").append(to_string(channelCount_));
  }
  if (sampleFrameRate_ != 0) {
    s.append("/rate=").append(to_string(sampleFrameRate_));
  }
  if (sampleFrameCount_ != 0) {
    s.append("/samples=").append(to_string(sampleFrameCount_));
  }
  if (getSampleFrameStride() * 8 != getBitsPerSample() * channelCount_) {
    s.append("/stride=").append(to_string(sampleFrameStride_));
  }
  return s;
}

size_t AudioContentBlockSpec::getBlockSize() const {
  return audioFormat_ == AudioFormat::PCM ? getPcmBlockSize() : ContentBlock::kSizeUnknown;
}

size_t AudioContentBlockSpec::getPcmBlockSize() const {
  uint8_t stride = getSampleFrameStride();
  if (stride > 0 && sampleFrameCount_ > 0) {
    return stride * sampleFrameCount_;
  }
  return ContentBlock::kSizeUnknown;
}

string AudioContentBlockSpec::getSampleFormatAsString() const {
  return AudioSampleFormatConverter::toString(sampleFormat_);
}

bool AudioContentBlockSpec::isLittleEndian(AudioSampleFormat sampleFormat) {
  switch (sampleFormat) {
    case AudioSampleFormat::S8:
    case AudioSampleFormat::U8:
    case AudioSampleFormat::A_LAW: // well it does not matter for these..
    case AudioSampleFormat::MU_LAW:
    case AudioSampleFormat::S16_LE:
    case AudioSampleFormat::U16_LE:
    case AudioSampleFormat::S24_LE:
    case AudioSampleFormat::U24_LE:
    case AudioSampleFormat::S32_LE:
    case AudioSampleFormat::U32_LE:
    case AudioSampleFormat::F32_LE:
    case AudioSampleFormat::F64_LE:
      return true;
    case AudioSampleFormat::S16_BE:
    case AudioSampleFormat::U16_BE:
    case AudioSampleFormat::S24_BE:
    case AudioSampleFormat::U24_BE:
    case AudioSampleFormat::S32_BE:
    case AudioSampleFormat::U32_BE:
    case AudioSampleFormat::F32_BE:
    case AudioSampleFormat::F64_BE:
      return false;
    // Don't use default to force every enum value to be explicitly specified
    case AudioSampleFormat::UNDEFINED:
    case AudioSampleFormat::COUNT:
      return true;
  }
  return true; // required by some compilers
}

bool AudioContentBlockSpec::isIEEEFloat(AudioSampleFormat sampleFormat) {
  switch (sampleFormat) {
    case AudioSampleFormat::F32_LE:
    case AudioSampleFormat::F64_LE:
    case AudioSampleFormat::F32_BE:
    case AudioSampleFormat::F64_BE:
      return true;
    default:
      return false;
  }
}

uint8_t AudioContentBlockSpec::getBitsPerSample(AudioSampleFormat sampleFormat) {
  switch (sampleFormat) {
    case AudioSampleFormat::S8:
    case AudioSampleFormat::U8:
    case AudioSampleFormat::A_LAW:
    case AudioSampleFormat::MU_LAW:
      return 8;
    case AudioSampleFormat::S16_LE:
    case AudioSampleFormat::S16_BE:
    case AudioSampleFormat::U16_LE:
    case AudioSampleFormat::U16_BE:
      return 16;
    case AudioSampleFormat::S24_LE:
    case AudioSampleFormat::S24_BE:
    case AudioSampleFormat::U24_LE:
    case AudioSampleFormat::U24_BE:
      return 24;
    case AudioSampleFormat::S32_LE:
    case AudioSampleFormat::S32_BE:
    case AudioSampleFormat::U32_LE:
    case AudioSampleFormat::U32_BE:
    case AudioSampleFormat::F32_LE:
    case AudioSampleFormat::F32_BE:
      return 32;
    case AudioSampleFormat::F64_LE:
    case AudioSampleFormat::F64_BE:
      return 64;
    // Don't use default to force every enum value to be explicitly specified
    case AudioSampleFormat::UNDEFINED:
    case AudioSampleFormat::COUNT:
      return 0;
  }
  return 0; // required by some compilers
}

uint8_t AudioContentBlockSpec::getBytesPerSample(AudioSampleFormat sampleFormat) {
  return (getBitsPerSample(sampleFormat) + 7) >> 3;
}

uint8_t AudioContentBlockSpec::getSampleFrameStride() const {
  if (sampleFrameStride_ != 0) {
    return sampleFrameStride_;
  }
  return getBytesPerSample() * channelCount_;
}

ContentBlock::ContentBlock(ImageFormat imageFormat, uint32_t width, uint32_t height)
    : contentType_(ContentType::IMAGE), imageSpec_(imageFormat, width, height) {}

ContentBlock::ContentBlock(
    const string& codecName,
    uint8_t codecQuality,
    PixelFormat pixelFormat,
    uint32_t width,
    uint32_t height,
    uint32_t stride,
    uint32_t stride2)
    : contentType_(ContentType::IMAGE),
      imageSpec_(codecName, codecQuality, pixelFormat, width, height, stride, stride2) {}

ContentBlock::ContentBlock(
    PixelFormat pixelFormat,
    uint32_t width,
    uint32_t height,
    uint32_t stride,
    uint32_t stride2)
    : contentType_(ContentType::IMAGE), imageSpec_(pixelFormat, width, height, stride, stride2) {}

ContentBlock::ContentBlock(const ImageContentBlockSpec& imageSpec, size_t size)
    : contentType_(ContentType::IMAGE), size_{size}, imageSpec_{imageSpec} {}

ContentBlock::ContentBlock(
    const ContentBlock& imageContentBlock,
    double keyFrameTimestamp,
    uint32_t keyFrameIndex)
    : contentType_(ContentType::IMAGE),
      size_{imageContentBlock.getBlockSize()},
      imageSpec_{imageContentBlock.image(), keyFrameTimestamp, keyFrameIndex} {}

ContentBlock::ContentBlock(AudioFormat audioFormat, uint8_t channelCount)
    : contentType_{ContentType::AUDIO}, audioSpec_{audioFormat, channelCount} {}

ContentBlock::ContentBlock(const AudioContentBlockSpec& audioSpec, size_t size)
    : contentType_{ContentType::AUDIO}, size_{size}, audioSpec_{audioSpec} {}

ContentBlock::ContentBlock(
    AudioFormat audioFormat,
    AudioSampleFormat sampleFormat,
    uint8_t numChannels,
    uint8_t sampleFrameStride,
    uint32_t sampleRate,
    uint32_t sampleCount,
    uint8_t stereoPairCount)
    : contentType_(ContentType::AUDIO),
      audioSpec_(
          audioFormat,
          sampleFormat,
          numChannels,
          sampleFrameStride,
          sampleRate,
          sampleCount,
          stereoPairCount) {}

ContentBlock::ContentBlock(ContentType type, size_t size) : contentType_(type), size_(size) {
  switch (contentType_) {
    case ContentType::IMAGE:
      imageSpec_.clear();
      break;
    case ContentType::AUDIO:
      audioSpec_.clear();
      break;
    default: // nothing to do
      break;
  }
}

ContentBlock::ContentBlock(const string& formatStr) {
  ContentParser parser(formatStr, '/');
  contentType_ = ContentTypeFormatConverter::toEnum(parser.str.c_str());
  parser.next();
  uint32_t size = 0;
  if (sscanf(parser.str.c_str(), "size=%u", &size) == 1) {
    size_ = size;
    parser.next();
  }
  switch (contentType_) {
    case ContentType::IMAGE:
      imageSpec_.set(parser);
      break;
    case ContentType::AUDIO:
      audioSpec_.set(parser);
      break;
    case ContentType::CUSTOM:
    case ContentType::DATA_LAYOUT:
    case ContentType::EMPTY:
    default:
      if (!parser.str.empty()) {
        XR_LOGE("Unknown content block specification '{}'.", parser.str.c_str());
      }
  }
}

string ContentBlock::asString() const {
  string s;
  s.reserve(120);
  s.append(ContentTypeFormatConverter::toString(contentType_));
  if (size_ != kSizeUnknown) {
    s.append("/size=").append(to_string(size_));
  }
  string subtype;
  switch (contentType_) {
    case ContentType::IMAGE:
      subtype = imageSpec_.asString();
      break;
    case ContentType::AUDIO:
      subtype = audioSpec_.asString();
      break;
    default:
      break;
  }
  if (!subtype.empty()) {
    s.append("/").append(subtype);
  }
  return s;
}

size_t ContentBlock::getBlockSize() const {
  if (contentType_ == ContentType::EMPTY) {
    return 0;
  }
  size_t size = size_;
  if (size == kSizeUnknown) {
    // check specific content type
    switch (contentType_) {
      case ContentType::IMAGE:
        size = imageSpec_.getBlockSize();
        break;
      case ContentType::AUDIO:
        size = audioSpec_.getBlockSize();
        break;
      default:
        break;
    }
  }
  return size;
}

RecordFormat ContentBlock::operator+(const ContentBlock& other) const {
  return {*this, other};
}

const ImageContentBlockSpec& ContentBlock::image() const {
  XR_VERIFY(contentType_ == ContentType::IMAGE);
  return imageSpec_;
}

const AudioContentBlockSpec& ContentBlock::audio() const {
  XR_VERIFY(contentType_ == ContentType::AUDIO);
  return audioSpec_;
}

void RecordFormat::set(const string& format) {
  blocks_.clear();
  ContentParser parser(format, '+');
  do {
    blocks_.emplace_back(parser.str); // do this at least once to get one block!
  } while (parser.next());
}

string RecordFormat::asString() const {
  if (blocks_.empty()) {
    return ContentBlock(ContentType::EMPTY).asString();
  }
  string s = blocks_.begin()->asString();
  for (auto iter = ++blocks_.begin(); iter != blocks_.end(); ++iter) {
    s.append("+").append(iter->asString());
  }
  return s;
}

size_t RecordFormat::getRecordSize() const {
  return getRemainingBlocksSize(0);
}

size_t RecordFormat::getRemainingBlocksSize(size_t firstBlock) const {
  size_t size = 0;
  for (size_t k = firstBlock; k < blocks_.size(); ++k) {
    size_t blockSize = blocks_[k].getBlockSize();
    if (blockSize == ContentBlock::kSizeUnknown) {
      return ContentBlock::kSizeUnknown;
    }
    size += blockSize;
  }
  return size;
}

size_t RecordFormat::getUsedBlocksCount() const {
  // Make sure we don't count empty blocks at the end
  for (size_t k = blocks_.size(); k > 0; --k) {
    if (blocks_[k - 1].getContentType() != ContentType::EMPTY) {
      return k;
    }
  }
  return 0;
}

size_t RecordFormat::getBlocksOfTypeCount(ContentType type) const {
  size_t count = 0;
  for (const auto& block : blocks_) {
    if (block.getContentType() == type) {
      ++count;
    }
  }
  return count;
}

size_t RecordFormat::getBlockSize(size_t blockIndex, size_t remainingSize) const {
  const size_t blockSize = blocks_[blockIndex].getBlockSize();
  if (blockSize != ContentBlock::kSizeUnknown) {
    return blockSize <= remainingSize ? blockSize : ContentBlock::kSizeUnknown;
  }
  const size_t remainingBlockSize = getRemainingBlocksSize(blockIndex + 1);
  if (remainingBlockSize != ContentBlock::kSizeUnknown && remainingBlockSize <= remainingSize) {
    return remainingSize - remainingBlockSize;
  }
  return ContentBlock::kSizeUnknown;
}

// These definitions are not meant to be used by anyone outside of these helper methods
static const char* const kRecordFormatTagPrefix = "RF:";
static const char* const kDataLayoutTagPrefix = "DL:";
static const char kFieldSeparator = ':';

string RecordFormat::getRecordFormatTagName(Record::Type recordType, uint32_t formatVersion) {
  string s;
  s.reserve(30);
  s.append(kRecordFormatTagPrefix, 3)
      .append(Record::typeName(recordType))
      .push_back(kFieldSeparator);
  s.append(to_string(formatVersion));
  return s;
}

string RecordFormat::getDataLayoutTagName(Record::Type type, uint32_t version, size_t blockIndex) {
  string s;
  s.reserve(30);
  s.append(kDataLayoutTagPrefix, 3).append(Record::typeName(type)).push_back(kFieldSeparator);
  s.append(to_string(version)).push_back(kFieldSeparator);
  s.append(to_string(blockIndex));
  return s;
}

// reads a record type name, moves the string pointer & returns true on success
static bool getRecordType(const char*& str, Record::Type& outRecordType) {
  static const char* kData = Record::typeName(Record::Type::DATA);
  static const size_t kDataLength = strlen(kData);
  static const char* kConfiguration = Record::typeName(Record::Type::CONFIGURATION);
  static const size_t kConfigurationLength = strlen(kConfiguration);
  static const char* kState = Record::typeName(Record::Type::STATE);
  static const size_t kStateLength = strlen(kState);

  if (strncmp(str, kData, kDataLength) == 0) {
    str += kDataLength;
    outRecordType = Record::Type::DATA;
    return true;
  }
  if (strncmp(str, kConfiguration, kConfigurationLength) == 0) {
    str += kConfigurationLength;
    outRecordType = Record::Type::CONFIGURATION;
    return true;
  }
  if (strncmp(str, kState, kStateLength) == 0) {
    str += kStateLength;
    outRecordType = Record::Type::STATE;
    return true;
  }
  outRecordType = Record::Type::UNDEFINED;
  return false;
}

bool RecordFormat::parseRecordFormatTagName(
    const string& tagName,
    Record::Type& recordType,
    uint32_t& outFormatVersion) {
  static const size_t tagPrefixLength = strlen(kRecordFormatTagPrefix);
  // quick test for the tag prefix, to stop early
  const char* str = tagName.c_str();
  if (strncmp(str, kRecordFormatTagPrefix, tagPrefixLength) != 0 ||
      !getRecordType(str += tagPrefixLength, recordType)) {
    return false;
  }
  if (*str != kFieldSeparator) {
    return false;
  }
  str += 1;
  bool parseSuccess = helpers::readUInt32(str, outFormatVersion);
  if (!parseSuccess) {
    XR_LOGE("Failed to parse '{}'.", str);
  }
  return parseSuccess && *str == 0;
}

bool RecordFormat::addRecordFormat(
    map<string, string>& inOutRecordFormatRegister,
    Record::Type recordType,
    uint32_t formatVersion,
    const RecordFormat& format,
    const vector<const DataLayout*>& layouts) {
  inOutRecordFormatRegister[RecordFormat::getRecordFormatTagName(recordType, formatVersion)] =
      format.asString();
  for (size_t index = 0; index < layouts.size(); ++index) {
    const DataLayout* layout = layouts[index];
    if (layout != nullptr) {
      inOutRecordFormatRegister[RecordFormat::getDataLayoutTagName(
          recordType, formatVersion, index)] = layout->asJson();
    }
  }
  bool allGood = true;
  // It's too easy to tell in RecordFormat that you're using a DataLayout,
  // and not specify that DataLayout (or at the wrong index). Let's warn the VRS user!
  size_t usedBlocks = format.getUsedBlocksCount();
  size_t maxIndex = max<size_t>(usedBlocks, layouts.size());
  for (size_t index = 0; index < maxIndex; ++index) {
    if (index < usedBlocks &&
        format.getContentBlock(index).getContentType() == ContentType::DATA_LAYOUT) {
      if (index >= layouts.size() || layouts[index] == nullptr) {
        XR_LOGE(
            "Missing DataLayout definition for Type:{}, FormatVersion:{}, Block #{}",
            toString(recordType),
            formatVersion,
            index);
        allGood = false;
      }
    } else if (index < layouts.size() && layouts[index] != nullptr) {
      XR_LOGE(
          "DataLayout definition provided from non-DataLayout block. "
          "Type: {}, FormatVersion:{}, Layout definition index:{}",
          toString(recordType),
          formatVersion,
          index);
      allGood = false;
    }
  }
  return allGood;
}

void RecordFormat::getRecordFormats(
    const map<string, string>& recordFormatRegister,
    RecordFormatMap& outFormats) {
  for (const auto& tag : recordFormatRegister) {
    Record::Type recordType{};
    uint32_t formatVersion = 0;
    if (RecordFormat::parseRecordFormatTagName(tag.first, recordType, formatVersion) &&
        outFormats.find({recordType, formatVersion}) == outFormats.end()) {
      outFormats[{recordType, formatVersion}].set(tag.second);
    }
  }
}

unique_ptr<DataLayout> RecordFormat::getDataLayout(
    const map<string, string>& recordFormatRegister,
    const ContentBlockId& blockId) {
  string tagName = RecordFormat::getDataLayoutTagName(
      blockId.getRecordType(), blockId.getFormatVersion(), blockId.getBlockIndex());
  const auto iter = recordFormatRegister.find(tagName);
  if (iter != recordFormatRegister.end()) {
    return DataLayout::makeFromJson(iter->second);
  }
  return nullptr;
}

string toString(ContentType contentType) {
  return ContentTypeFormatConverter::toString(contentType);
}

string toString(ImageFormat imageFormat) {
  return ImageFormatConverter::toString(imageFormat);
}

string toString(PixelFormat pixelFormat) {
  return PixelFormatConverter::toString(pixelFormat);
}

string toString(AudioFormat audioFormat) {
  return AudioFormatConverter::toString(audioFormat);
}

string toString(AudioSampleFormat audioSampleFormat) {
  return AudioSampleFormatConverter::toString((audioSampleFormat));
}

} // namespace vrs
