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

#include "PixelFrame.h"

#include <map>
#include <mutex>
#include <thread>

#ifdef JXL_IS_AVAILABLE
#include <jxl/color_encoding.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include <jxl/encode.h>
#include <jxl/encode_cxx.h>
#include <jxl/types.h>
#endif

#define DEFAULT_LOG_CHANNEL "PixelFrameJxl"
#include <logging/Checks.h>
#include <logging/Log.h>
#include <logging/Verify.h>

#ifdef JXL_IS_AVAILABLE

/// About Jpeg-XL support
/// This code requires Jpeg-XL v0.7 (won't compile with v0.61)
/// Jpeg-XL is support is still considered experimental, as the API still changes in breaking ways,
/// which is why support is disabled by default in VRS OSS.
/// As of Sep 23, 2022, get jpg-xl from https://github.com/libjxl and use the v0.7.x branch

#include <vrs/helpers/MemBuffer.h>

#define DEC_CHECK(operation_)                         \
  do {                                                \
    JxlDecoderStatus status_ = operation_;            \
    if (status_ != JXL_DEC_SUCCESS) {                 \
      XR_LOGE("{} failed: {}", #operation_, status_); \
      return false;                                   \
    }                                                 \
  } while (false)

#define JXL_EFFORT 3

#define ENC_CHECK(operation_)                         \
  do {                                                \
    JxlEncoderStatus status_ = operation_;            \
    if (status_ != JXL_ENC_SUCCESS) {                 \
      XR_LOGE("{} failed: {}", #operation_, status_); \
      return false;                                   \
    }                                                 \
  } while (false)

using namespace std;

namespace {

template <class T>
T& getThreadObject() {
  static mutex sMutex;
  unique_lock<mutex> locker(sMutex);
  static map<thread::id, T> sObjects;
  return sObjects[this_thread::get_id()];
}

JxlDecoder* getThreadJxlDecoder() {
  auto& decoder = getThreadObject<JxlDecoderPtr>();
  if (decoder) {
    JxlDecoderReset(decoder.get());
  } else {
    decoder = JxlDecoderMake(nullptr);
  }
  return decoder.get();
}

JxlEncoder* getThreadJxlEncoder() {
  auto& encoder = getThreadObject<JxlEncoderPtr>();
  if (encoder) {
    JxlEncoderReset(encoder.get());
  } else {
    encoder = JxlEncoderMake(nullptr);
  }
  return encoder.get();
}

inline float percent_to_butteraugli_distance(float quality) {
  // Quality calculation inspired by cjxl.cc
  // Extended to work meaningfully between 99.99 and 99.999, so with quality = 99.999,
  // the file size is now getting close to that of lossless.
  // Improved continuity around the 26-30 range.
  float butteraugli_distance;
  if (quality >= 100) {
    butteraugli_distance = 0;
  } else if (quality >= 99.99) {
    // linear, connecting to 100% <-> 0
    butteraugli_distance = 0.0007 + (100 - quality) * 10;
  } else if (quality >= 26.8) {
    // linear, fairly soft changes
    butteraugli_distance = 0.1 + (100 - quality) * 0.09;
  } else {
    // exponential, limit 15 max
    butteraugli_distance = min<float>(15.f, 6.4 + pow(2.5, (30 - quality) / 5.0f) / 6.25f);
  }
  return butteraugli_distance;
}

} // namespace
#endif

namespace vrs::utils {

bool PixelFrame::readJxlFrame(RecordReader* reader, const uint32_t sizeBytes) {
  if (sizeBytes == 0) {
    return false; // empty image
  }
  // read JPEG-XL data
  vector<uint8_t> jxlBuf;
  jxlBuf.resize(sizeBytes);
  if (!XR_VERIFY(reader->read(jxlBuf.data(), sizeBytes) == 0)) {
    return false;
  }
  return readJxlFrame(jxlBuf);
}

bool PixelFrame::readJxlFrame(const vector<uint8_t>& jxlBuf, bool decodePixels) {
#ifdef JXL_IS_AVAILABLE
  auto dec = getThreadJxlDecoder();
  if (!XR_VERIFY(dec != nullptr)) {
    return false;
  }
  DEC_CHECK(JxlDecoderSubscribeEvents(
      dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE));

  DEC_CHECK(JxlDecoderSetInput(dec, jxlBuf.data(), jxlBuf.size()));
  JxlDecoderCloseInput(dec);

  JxlPixelFormat format = {3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 /* alignment */};
  for (JxlDecoderStatus status; (status = JxlDecoderProcessInput(dec)) != JXL_DEC_SUCCESS;) {
    switch (status) {
      case JXL_DEC_ERROR:
        XR_LOGE("JPEG XL decoder: Decoder error");
        return false;

      case JXL_DEC_NEED_MORE_INPUT:
        XR_LOGE("JPEG XL decoder: need more input");
        return false;

      case JXL_DEC_COLOR_ENCODING: {
        JxlColorEncoding colorEncoding;
        if (JxlDecoderGetColorAsEncodedProfile(
                dec, &format, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &colorEncoding) ==
                JXL_DEC_SUCCESS &&
            colorEncoding.color_space == JXL_COLOR_SPACE_GRAY) {
          colorEncoding.gamma = 0.5;
          colorEncoding.transfer_function = JXL_TRANSFER_FUNCTION_GAMMA;
          JxlDecoderSetPreferredColorProfile(dec, &colorEncoding);
        }
      } break;

      case JXL_DEC_BASIC_INFO: {
        JxlBasicInfo info = {};
        DEC_CHECK(JxlDecoderGetBasicInfo(dec, &info));
        if (info.exponent_bits_per_sample != 0) {
          XR_LOGE("jxl floating point pixel format not supported (you can fix that)");
          return false;
        }
        if (info.num_extra_channels != 0 && info.num_extra_channels != 1) {
          XR_LOGE("Unexpected number of extra channels: {}", info.num_extra_channels);
          return false;
        }
        if (info.num_color_channels != 1 && info.num_color_channels != 3) {
          XR_LOGE("Unexpected number of color channels: {}", info.num_color_channels);
          return false;
        }
        format.num_channels = info.num_color_channels + info.num_extra_channels;
        bool supportedFormat = true;
        switch (info.bits_per_sample) {
          case 8: {
            format.data_type = JXL_TYPE_UINT8;
            switch (format.num_channels) {
              case 1:
                init(PixelFormat::GREY8, info.xsize, info.ysize);
                break;
              case 3:
                init(PixelFormat::RGB8, info.xsize, info.ysize);
                break;
              case 4:
                init(PixelFormat::RGBA8, info.xsize, info.ysize);
                break;
              default:
                supportedFormat = false;
            }
          } break;
          case 10: {
            format.data_type = JXL_TYPE_UINT16;
            switch (format.num_channels) {
              case 1:
                init(PixelFormat::GREY10, info.xsize, info.ysize);
                break;
              case 3:
                init(PixelFormat::RGB10, info.xsize, info.ysize);
                break;
              default:
                supportedFormat = false;
            }
          } break;
          default:
            supportedFormat = false;
        }
        if (!supportedFormat) {
          XR_LOGE(
              "Unexpected combo of color/extra channels {}+{} channels @ {} bits per sample",
              info.num_color_channels,
              info.num_extra_channels,
              info.bits_per_sample);
          return false;
        }
        if (!decodePixels) {
          JxlDecoderReset(dec);
          return true;
        }
      } break;

      case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
        size_t buffer_size;
        DEC_CHECK(JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
        vector<uint8_t>& pixels = getBuffer();
        if (buffer_size != pixels.size()) {
          XR_LOGE(
              "Unexpected output buffer size: {} bytes vs. {} expected",
              buffer_size,
              pixels.size());
          return false;
        }
        DEC_CHECK(JxlDecoderSetImageOutBuffer(dec, &format, pixels.data(), buffer_size));
      } break;

      default:
        break;
    }
  }
  return true;
#else
  XR_LOGW_EVERY_N_SEC(10, "jpeg-xl support is not enabled.");
  return false;
#endif
}

bool PixelFrame::jxlCompress(
    vector<uint8_t>& outBuffer,
    float quality,
    bool percentNotDistance,
    int effort) {
  return jxlCompress(imageSpec_, frameBytes_, outBuffer, quality, percentNotDistance, effort);
}

bool PixelFrame::jxlCompress(
    const ImageContentBlockSpec& pixelSpec,
    const vector<uint8_t>& pixels,
    vector<uint8_t>& outBuffer,
    float quality,
    bool percentNotDistance,
    int effort) {
#ifdef JXL_IS_AVAILABLE
  // Image quality, between 8.5 and 100 (lossless), with floating point resolution,
  // so 99 is less than 99.5 which is less than 99.9, which is also less than 99.99.
  // 99.995 is the best usable lossy quality setting, and 100 is a step jump to lossless.
  // Sets the decoding speed tier for the provided options. Minimum is 0
  // (slowest to decode, best quality/density), and maximum is 4 (fastest to
  // decode, at the cost of some quality/density). Default is 0.
  int decoding_speed_tier = 0;

  const float butteraugli = percentNotDistance ? percent_to_butteraugli_distance(quality) : quality;

  JxlEncoder* enc = getThreadJxlEncoder();
  if (!XR_VERIFY(enc != nullptr)) {
    return false;
  }

  const uint32_t channels = pixelSpec.getChannelCountPerPixel();

  JxlPixelFormat pixel_format = {channels, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};

  JxlBasicInfo basic_info = {};
  JxlEncoderInitBasicInfo(&basic_info); // Default set to RGB8
  basic_info.xsize = pixelSpec.getWidth();
  basic_info.ysize = pixelSpec.getHeight();
  basic_info.num_color_channels = channels;
  switch (pixelSpec.getPixelFormat()) {
    case PixelFormat::GREY8:
    case PixelFormat::RGB8:
      basic_info.bits_per_sample = 8;
      pixel_format.data_type = JXL_TYPE_UINT8;
      break;

    case PixelFormat::GREY10:
    case PixelFormat::RGB10:
      basic_info.bits_per_sample = 10;
      pixel_format.data_type = JXL_TYPE_UINT16;
      break;

    default:
      return false;
  }
  basic_info.uses_original_profile = (butteraugli <= 0) ? JXL_TRUE : JXL_FALSE;

  ENC_CHECK(JxlEncoderSetBasicInfo(enc, &basic_info));

  JxlColorEncoding color_encoding = {};
  JxlColorEncodingSetToSRGB(
      &color_encoding,
      /*is_gray=*/pixel_format.num_channels < 3);
  ENC_CHECK(JxlEncoderSetColorEncoding(enc, &color_encoding));

  auto settings = JxlEncoderFrameSettingsCreate(enc, nullptr);
  ENC_CHECK(JxlEncoderFrameSettingsSetOption(settings, JXL_ENC_FRAME_SETTING_EFFORT, effort));
  ENC_CHECK(JxlEncoderFrameSettingsSetOption(
      settings, JXL_ENC_FRAME_SETTING_DECODING_SPEED, decoding_speed_tier));
#if 0
    // code to tune quality to butteraugli_distance conversion
    float last = 0;
    for (quality = 99.985; quality < 100; quality += 0.0001) {
      float butteraugli_distance = percent_to_butteraugli_distance(quality);
      ENC_CHECK(JxlEncoderSetFrameDistance(settings, butteraugli_distance));
      fmt::print(
          "Quality: {} Distance: {} Gap: {}\n",
          quality,
          butteraugli_distance,
          last - butteraugli_distance);
      last = butteraugli_distance;
    }
#endif
  if (butteraugli <= 0) {
    ENC_CHECK(JxlEncoderSetFrameLossless(settings, JXL_TRUE));
  } else {
    ENC_CHECK(JxlEncoderSetFrameDistance(settings, butteraugli));
  }
  ENC_CHECK(JxlEncoderAddImageFrame(settings, &pixel_format, pixels.data(), pixels.size()));
  JxlEncoderCloseInput(enc);

  static atomic<size_t> sStartSize{256 * 1024}; // shared between threads
  size_t allocSize = sStartSize.load(memory_order_relaxed);
  helpers::MemBuffer memBuffer(allocSize);

  uint8_t* outData{};
  JxlEncoderStatus encoderStatus;
  do {
    size_t allocatedSize = memBuffer.allocateSpace(outData, allocSize);
    size_t remainingSize = allocatedSize;
    encoderStatus = JxlEncoderProcessOutput(enc, &outData, &remainingSize);
    if (JXL_ENC_SUCCESS != encoderStatus && JXL_ENC_NEED_MORE_OUTPUT != encoderStatus) {
      throw runtime_error("JxlEncoderProcessOutput failed");
    }
    memBuffer.addAllocatedSpace(allocatedSize - remainingSize);
  } while (encoderStatus == JXL_ENC_NEED_MORE_OUTPUT);
  memBuffer.getData(outBuffer);
  if (outBuffer.size() > sStartSize.load(memory_order_relaxed)) {
    sStartSize.store(outBuffer.size() + outBuffer.size() / 100, memory_order_relaxed);
  }
  return true;
#else
  XR_LOGW_EVERY_N_SEC(10, "jpeg-xl support is not enabled.");
  return false;
#endif
}

} // namespace vrs::utils
