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

//
// See FFmpegEncode.h for details.

#include "FFmpegUtils.h"

#include <cstring>

#include <array>
#include <sstream>

#include "Codecs.h"

namespace xprs {

Picture::Picture() {
  _avFrame = av_frame_alloc();
  if (_avFrame == nullptr) {
    throw INVOKE_CODEC_EXCEPTION_MESSAGE("av_frame_alloc failed");
  }
}

Picture::~Picture() {
  av_frame_free(&_avFrame);
}

CodecException::CodecException(const char* file, int line, int error) {
  constexpr size_t errorBufSize = 1024;
  static char errorBuf[errorBufSize];
  av_strerror(error, errorBuf, errorBufSize);

  _file = fileName(file);
  _line = line;
  _msg = errorBuf;
  _error = error;
}

CodecException::CodecException(const char* file, int line, const char* msg) {
  _file = fileName(file);
  _line = line;
  _msg = msg;
  _error = 0;
}

const char* CodecException::what() const noexcept {
  std::stringstream errMsg;
  errMsg << "Error at " << _file << ':' << _line << ": " << _msg << std::endl;

  errString = errMsg.str();
  return errString.c_str();
}

int CodecException::error() const noexcept {
  return _error;
}

const char* CodecException::fileName(const char* path) {
  auto index = strlen(path);

  while (index) {
    index--;
    if (*(path + index) == '/' || *(path + index) == '\\') {
      index++;
      break;
    }
  }

  return (path + index);
}

CodecPreset mapToCodecPreset(const char* preset, AVCodecID id, const char* avcodecName) {
  // Map speed string to index: 0=slow, 1=medium, 2=fast
  int speed = 1;
  if (preset == nullptr || strcmp(preset, "medium") == 0) {
    speed = 1;
  } else if (strcmp(preset, "slow") == 0) {
    speed = 0;
  } else if (strcmp(preset, "fast") == 0) {
    speed = 2;
  } else {
    throw INVOKE_CODEC_EXCEPTION_MESSAGE("unknown preset");
  }

  bool isNv =
      avcodecName && (avcodecName == kNvH264EncoderName || avcodecName == kNvH265EncoderName);

  // Preset tables indexed by speed: [slow, medium, fast]
  constexpr std::array<const char*, 3> kH264Presets = {"slower", "medium", "superfast"};
  constexpr std::array<const char*, 3> kH264NvPresets = {"slow", "medium", "fast"};
  constexpr std::array<const char*, 3> kH265Presets = {"slower", "medium", "superfast"};
  constexpr std::array<const char*, 3> kH265NvPresets = {"slow", "medium", "fast"};
  constexpr std::array<const char*, 3> kAV1Presets = {"8", "10", "12"};
  constexpr std::array<int, 3> kVP9CpuUsed = {1, 4, 5};

  CodecPreset result{nullptr};
  // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
  switch (id) {
    case AV_CODEC_ID_H264:
      result.preset = isNv ? kH264NvPresets[speed] : kH264Presets[speed];
      break;
    case AV_CODEC_ID_H265:
      result.preset = isNv ? kH265NvPresets[speed] : kH265Presets[speed];
      break;
    case AV_CODEC_ID_VP9:
      result.cpuUsed = kVP9CpuUsed[speed];
      break;
    case AV_CODEC_ID_AV1:
      result.preset = kAV1Presets[speed];
      break;
    default:
      break;
  }

  return result;
}

} // namespace xprs
