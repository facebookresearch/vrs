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
#include <sstream>

#include "Codecs.h"

namespace xprs {

Picture::Picture() {
  _avFrame = av_frame_alloc();
  if (_avFrame == nullptr)
    throw INVOKE_CODEC_EXCEPTION_MESSAGE("av_frame_alloc failed");
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
  CodecPreset result{nullptr};

  if (strcmp(preset, "slow") == 0) {
    switch (id) {
      case AV_CODEC_ID_H264:
        if (avcodecName &&
            (avcodecName == kNvH264EncoderName || avcodecName == kNvH265EncoderName)) {
          result.preset = "slow";
        } else {
          result.preset = "slower";
        }
        break;
      case AV_CODEC_ID_H265:
        result.preset = "slower";
        break;
      case AV_CODEC_ID_VP9:
        result.cpuUsed = 1;
        break;
      case AV_CODEC_ID_AV1:
        result.preset = "8";
        break;
      default:
        break;
    }
  } else if (preset == nullptr || strcmp(preset, "medium") == 0) {
    switch (id) {
      case AV_CODEC_ID_H264:
        result.preset = "medium";
        break;
      case AV_CODEC_ID_H265:
        result.preset = "medium";
        break;
      case AV_CODEC_ID_VP9:
        result.cpuUsed = 4;
        break;
      case AV_CODEC_ID_AV1:
        result.preset = "10";
        break;
      default:
        break;
    }
  } else if (strcmp(preset, "fast") == 0) {
    switch (id) {
      case AV_CODEC_ID_H264:
        if (avcodecName &&
            (avcodecName == kNvH264EncoderName || avcodecName == kNvH265EncoderName)) {
          result.preset = "fast";
        } else {
          result.preset = "superfast";
        }
        break;
      case AV_CODEC_ID_H265:
        result.preset = "superfast";
        break;
      case AV_CODEC_ID_VP9:
        result.cpuUsed = 5;
        break;
      case AV_CODEC_ID_AV1:
        result.preset = "12";
        break;
      default:
        break;
    }
  } else {
    throw INVOKE_CODEC_EXCEPTION_MESSAGE("unknown preset");
  }

  return result;
}

} // namespace xprs
