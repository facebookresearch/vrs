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
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#endif

#define DEFAULT_LOG_CHANNEL "PixelFrameJxl"
#include <logging/Log.h>
#include <logging/Verify.h>

#ifdef JXL_IS_AVAILABLE

#define JXL_CHECK(operation_)                         \
  do {                                                \
    JxlDecoderStatus status_ = operation_;            \
    if (status_ != JXL_DEC_SUCCESS) {                 \
      XR_LOGE("{} failed: {}", #operation_, status_); \
      return false;                                   \
    }                                                 \
  } while (false)

using namespace std;

namespace {
map<thread::id, JxlDecoderPtr>& getDecoders() {
  static map<thread::id, JxlDecoderPtr> sEncoders;
  return sEncoders;
}

JxlDecoderPtr& getThreadDecoderSmartPtr() {
  static mutex sMutex;
  unique_lock<mutex> locker(sMutex);
  return getDecoders()[this_thread::get_id()];
}

JxlDecoder* getThreadDecoder() {
  auto& decoder = getThreadDecoderSmartPtr();
  if (decoder) {
    JxlDecoderReset(decoder.get());
  } else {
    decoder = JxlDecoderMake(nullptr);
  }
  return decoder.get();
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
  auto dec = getThreadDecoder();
  JXL_CHECK(JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE));

  JXL_CHECK(JxlDecoderSetInput(dec, jxlBuf.data(), jxlBuf.size()));
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

      case JXL_DEC_BASIC_INFO: {
        JxlBasicInfo info = {};
        JXL_CHECK(JxlDecoderGetBasicInfo(dec, &info));
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
            XR_LOGE(
                "Unexpected combo of color channels and extra channels: {}+{}",
                info.num_color_channels,
                info.num_extra_channels);
            return false;
        }
        if (info.bits_per_sample != 8) {
          XR_LOGE("Unsupported bit per sample: {}", info.bits_per_sample);
          return false;
        }
        if (!decodePixels) {
          JxlDecoderReset(dec);
          return true;
        }
      } break;

      case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
        size_t buffer_size;
        JXL_CHECK(JxlDecoderImageOutBufferSize(dec, &format, &buffer_size));
        vector<uint8_t>& pixels = getBuffer();
        if (buffer_size != pixels.size()) {
          XR_LOGE(
              "Unexpected output buffer size: {} bytes vs. {} expected",
              buffer_size,
              pixels.size());
          return false;
        }
        JXL_CHECK(JxlDecoderSetImageOutBuffer(dec, &format, pixels.data(), buffer_size));
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

} // namespace vrs::utils
